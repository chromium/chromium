// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Image fetching mechanism for Omnibox and Suggestions. */
public class OmniboxImageSupplier {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    private final Map<GURL, List<Callback<Bitmap>>> mPendingImageRequests;
    private int mDesiredFaviconWidthPx;
    private @NonNull RoundedIconGenerator mIconGenerator;
    private @Nullable LargeIconBridge mIconBridge;
    private @Nullable ImageFetcher mImageFetcher;
    private boolean mNativeInitialized;

    /**
     * Constructor.
     *
     * @param context An Android context.
     */
    public OmniboxImageSupplier(@NonNull Context context) {
        mDesiredFaviconWidthPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_favicon_size);

        int fallbackIconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        int fallbackIconColor = context.getColor(R.color.default_favicon_background_color);
        int fallbackIconTextSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mIconGenerator =
                new RoundedIconGenerator(
                        fallbackIconSize,
                        fallbackIconSize,
                        fallbackIconSize / 2,
                        fallbackIconColor,
                        fallbackIconTextSize);
        mPendingImageRequests = new HashMap<>();
    }

    /** Release any resources and deregister any callbacks created by this class. */
    public void destroy() {
        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
        }

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        mPendingImageRequests.clear();
    }

    /** Notify OmniboxImageSupplier that certain native-requiring calls are now ready for use. */
    public void onNativeInitialized() {
        mNativeInitialized = true;
    }

    /**
     * Notify that the current User profile has changed.
     *
     * @param profile Current user profile.
     */
    public void setProfile(Profile profile) {
        if (mIconBridge != null) {
            mIconBridge.destroy();
        }

        mIconBridge = new LargeIconBridge(profile);
        resetCache();

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
        }

        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_ONLY,
                        profile.getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool(),
                        MAX_IMAGE_CACHE_SIZE);
    }

    /**
     * Asynchronously retrieve favicon for a given url and deliver the result via supplied callback.
     * All fetches are done asynchronously, but the caller should expect a synchronous call in some
     * cases, eg if an icon cannot be retrieved because a corresponding provider is not available.
     *
     * @param url The url to retrieve a favicon for.
     * @param callback The callback that will be invoked with the result.
     */
    public void fetchFavicon(@NonNull GURL url, @NonNull Callback<Bitmap> callback) {
        if (mIconBridge == null) {
            callback.onResult(null);
            return;
        }

        // Note: LargeIconBridge will serve <null> right away if a fetch was previously made and was
        // unsuccessful.
        mIconBridge.getLargeIconForUrl(
                url,
                mDesiredFaviconWidthPx / 2,
                mDesiredFaviconWidthPx,
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    callback.onResult(icon);
                });
    }

    /**
     * Asynchronously generate favicon for a given url and deliver the result via supplied callback.
     *
     * @param url The url to generate a favicon for.
     * @param callback The callback that will be invoked with the result.
     */
    public void generateFavicon(@NonNull GURL url, @NonNull Callback<Bitmap> callback) {
        if (!mNativeInitialized) {
            callback.onResult(null);
            return;
        }

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Bitmap icon = mIconGenerator.generateIconForUrl(url);
                    callback.onResult(icon);
                });
    }

    /** Clear all cached entries. */
    public void resetCache() {
        if (mIconBridge != null) mIconBridge.createCache(MAX_IMAGE_CACHE_SIZE);
        if (mImageFetcher != null) mImageFetcher.clear();
        mPendingImageRequests.clear();
    }

    /**
     * Asynchronously retrieve image for supplied GURL. Calls to this method result with callback
     * being invoked if and only if the fetch was executed and was successful.
     *
     * @param url The url to retrieve a favicon for.
     * @param callback The callback that will be invoked with the result.
     */
    public void fetchImage(GURL url, @NonNull Callback<Bitmap> callback) {
        if (mImageFetcher == null || !url.isValid() || url.isEmpty()) {
            return;
        }

        // Do not make duplicate answer image requests for the same URL (to avoid generating
        // duplicate bitmaps for the same image).
        if (mPendingImageRequests.containsKey(url)) {
            mPendingImageRequests.get(url).add(callback);
            return;
        }

        var callbacks = new ArrayList<Callback<Bitmap>>();
        callbacks.add(callback);
        mPendingImageRequests.put(url, callbacks);

        ImageFetcher.Params params =
                ImageFetcher.Params.create(url, ImageFetcher.OMNIBOX_UMA_CLIENT_NAME);

        mImageFetcher.fetchImage(
                params,
                bitmap -> {
                    final var pendingCallbacks = mPendingImageRequests.remove(url);
                    // Callbacks may be erased when Omnibox interaction is over.
                    if (bitmap == null || pendingCallbacks == null) return;

                    for (int i = 0; i < pendingCallbacks.size(); i++) {
                        pendingCallbacks.get(i).onResult(bitmap);
                    }
                });
    }

    /**
     * Overrides RoundedIconGenerator for testing.
     *
     * @param generator RoundedIconGenerator to use
     */
    void setRoundedIconGeneratorForTesting(@NonNull RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }

    /** Overrides ImageFetcher instance for testing. */
    void setImageFetcherForTesting(@Nullable ImageFetcher fetcher) {
        mImageFetcher = fetcher;
    }
}
