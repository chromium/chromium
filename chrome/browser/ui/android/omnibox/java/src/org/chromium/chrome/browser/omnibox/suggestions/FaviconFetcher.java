// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.LruCache;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Favicon fetching mechanism with multi-step fallback functionality.
 * The class supports very simple caching mechanism which remembers what type of icon is available
 * for a given URL to reduce number of JNI and object creation calls.
 */
public class FaviconFetcher {
    /**
     * Maximum number of IconAvailability records to keep.
     * The cache used here is cheap (cost of one GURL and one int per entry), but
     * allows us to reduce number of JNI calls for URLs we already confirmed to have
     * no icons.
     */
    private static final int MAX_ICON_AVAILABILITY_RECORDS = 256;

    /** Variants of available Favicons. */
    @IntDef({FaviconType.NONE, FaviconType.REGULAR, FaviconType.SMALL, FaviconType.GENERATED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FaviconType {
        // No favicon or fallback icon is available.
        int NONE = 0;
        // A regular size favicon is available.
        int REGULAR = 1;
        // Regular size favicon is not available, but small favicon is.
        int SMALL = 2;
        // Neither regular nor small favicon is available - fall back to RoundedIconGenerator.
        int GENERATED = 3;
    }

    /** Receiver of the Favicon fetch. */
    public interface FaviconFetchCompleteListener {
        /**
         * Invoked when a favicon fetch is complete.
         * @param bitmap The resulting bitmap; may be null if favicon could not be retrieved for
         *         the supplied URL (reported with FaviconType.NONE).
         * @param type The type of a favicon that was retrieved, or FaviconType.NONE if favicon
         *         could not be retrieved/acquired from any source.
         */
        void onFaviconFetchComplete(@Nullable Bitmap bitmap, @FaviconType int type);
    }

    private final @NonNull LruCache<GURL, Integer> mFaviconTypeCache;
    private final @NonNull Supplier<LargeIconBridge> mIconBridgeSupplier;
    private int mDesiredFaviconWidthPx;
    private @NonNull RoundedIconGenerator mIconGenerator;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param iconBridgeSupplier Supplier of the LargeIconBridge used to fetch site favicons.
     */
    public FaviconFetcher(
            @NonNull Context context, @NonNull Supplier<LargeIconBridge> iconBridgeSupplier) {
        mIconBridgeSupplier = iconBridgeSupplier;
        mDesiredFaviconWidthPx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);

        int fallbackIconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        int fallbackIconColor = context.getColor(R.color.default_favicon_background_color);
        int fallbackIconTextSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mIconGenerator = new RoundedIconGenerator(fallbackIconSize, fallbackIconSize,
                fallbackIconSize / 2, fallbackIconColor, fallbackIconTextSize);

        mFaviconTypeCache = new LruCache(MAX_ICON_AVAILABILITY_RECORDS);
    }

    /**
     * Retrieve a favicon for a particular URL, retrying the fetch request with a smaller size if
     * the requested size does not yield any results.
     *
     * @param url The url to retrieve a favicon for.
     * @param allowGeneratedIcon When true, Fetcher will generate a favicon if all fetch attempts
     *         have failed.
     * @param callback The callback to invoke with a favicon, once the result is known.
     *         The callback will be invoked with fallback bitmap if favicon cannot be retrieved.
     */
    public void fetchFaviconWithBackoff(@NonNull GURL url, boolean allowGeneratedIcon,
            @NonNull FaviconFetchCompleteListener callback) {
        // Check for possible former records on the favicon to determine where to look for it.
        @FaviconType
        Integer faviconType = mFaviconTypeCache.get(url);
        if (faviconType == null) {
            faviconType = FaviconType.REGULAR;
        }

        // If there are no regular favicons, and the caller rejects generated ones, abort.
        if ((faviconType == FaviconType.GENERATED) && !allowGeneratedIcon) {
            callback.onFaviconFetchComplete(null, FaviconType.NONE);
            return;
        }

        FaviconFetchCompleteListener backoffListener = new FaviconFetchCompleteListener() {
            @Override
            public void onFaviconFetchComplete(@Nullable Bitmap icon, @FaviconType int type) {
                // Pass an icon directly to the callee if the fetch succeeded.
                if (icon != null) {
                    // Remember the type of an icon to avoid backoff the next time we need this
                    // icon.
                    mFaviconTypeCache.put(url, type);
                    callback.onFaviconFetchComplete(icon, type);
                    return;
                }

                // Back off and try again otherwise.
                switch (type) {
                    case FaviconType.REGULAR:
                        fetchFaviconType(url, FaviconType.SMALL, this);
                        break;

                    case FaviconType.SMALL:
                        // At this point it's safe to assume the only type we can offer is a
                        // generated icon. This is because REGULAR and SMALL icon fetches have
                        // already failed. This is a minor optimization that helps us reduce number
                        // of calls to LargeIconBridge. Note this is safe even if we don't want
                        // generated icons. This is because we won't be generating an icon in such
                        // case.
                        mFaviconTypeCache.put(url, FaviconType.GENERATED);

                        if (allowGeneratedIcon) {
                            fetchFaviconType(url, FaviconType.GENERATED, this);
                            break;
                        }
                        // fallthrough: we don't allow generated icons.

                    case FaviconType.GENERATED:
                    default:
                        callback.onFaviconFetchComplete(null, FaviconType.NONE);
                        break;
                }
            }
        };

        fetchFaviconType(url, faviconType, backoffListener);
    }

    /**
     * For a supplied url, retrieve a favicon of desired faviconType and deliver the result via the
     * supplied callback.
     * All fetches are done asynchronously, but the caller should expect a synchronous call in some
     * cases, eg if an icon cannot be retrieved because a corresponding provider is not available.
     *
     * @param url The url to retrieve a favicon for.
     * @param faviconType The specific type of favicon to retrieve.
     * @param callback The callback that will be invoked with the result.
     */
    private void fetchFaviconType(@NonNull GURL url, @FaviconType int faviconType,
            @NonNull FaviconFetchCompleteListener callback) {
        LargeIconBridge iconBridge = mIconBridgeSupplier.get();

        // Determine if we need LargeIconBridge for this fetch, and abort early if we can't fulfill
        // the request.
        if ((faviconType == FaviconType.REGULAR || faviconType == FaviconType.SMALL)
                && (iconBridge == null)) {
            callback.onFaviconFetchComplete(null, faviconType);
            return;
        }

        switch (faviconType) {
            case FaviconType.REGULAR:
                iconBridge.getLargeIconForUrl(url, mDesiredFaviconWidthPx,
                        (icon, fallbackColor, isFallbackColorDefault,
                                iconType) -> callback.onFaviconFetchComplete(icon, faviconType));
                return;

            case FaviconType.SMALL:
                iconBridge.getLargeIconForUrl(url, mDesiredFaviconWidthPx / 2,
                        (icon, fallbackColor, isFallbackColorDefault,
                                iconType) -> callback.onFaviconFetchComplete(icon, faviconType));
                return;

            case FaviconType.GENERATED:
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                    Bitmap icon = mIconGenerator.generateIconForUrl(url);
                    callback.onFaviconFetchComplete(icon, faviconType);
                });
                return;

            default:
                assert false : "Invalid favicon type requested: " + faviconType;
                callback.onFaviconFetchComplete(null, faviconType);
                return;
        }
    }

    /** Clear all cached entries. */
    public void clearCache() {
        mFaviconTypeCache.evictAll();
    }

    /**
     * Overrides RoundedIconGenerator for testing.
     * @param generator RoundedIconGenerator to use.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setRoundedIconGeneratorForTesting(@NonNull RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }

    /**
     * Overrides desired favicon size for testing.
     * @param desiredFaviconSizePx Desired favicon size in pixels.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setDesiredFaviconSizeForTesting(int desiredFaviconSizePx) {
        mDesiredFaviconWidthPx = desiredFaviconSizePx;
    }
}
