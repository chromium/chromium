// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.OvalShape;
import android.graphics.drawable.shapes.PathShape;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Image fetching mechanism for Omnibox and Suggestions. */
@NullMarked
public class OmniboxImageSupplier {
    private static final String TAG = "OmniboxImageSupplier";
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    @IntDef({FallbackIconType.ROUNDED_LETTER, FallbackIconType.GLOBE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FallbackIconType {
        int ROUNDED_LETTER = 0;
        int GLOBE = 1;
    }

    private final Context mContext;
    private final Map<GURL, List<Callback<Drawable>>> mPendingImageRequests;
    private final Map<String, Drawable.ConstantState> mDrawableCache;
    private Drawable.@Nullable ConstantState mBackgroundDrawableState;
    private final int mDesiredFaviconWidthPx;
    private final int mFallbackIconSize;
    private final int mFallbackIconColor;
    private final int mFallbackIconTextSize;
    private @Nullable LargeIconBridge mIconBridge;
    private @Nullable ImageFetcher mImageFetcher;
    private boolean mNativeInitialized;

    /**
     * Constructor.
     *
     * @param context An Android context.
     */
    public OmniboxImageSupplier(Context context) {
        mContext = context;
        mDesiredFaviconWidthPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_favicon_size);

        mFallbackIconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        mFallbackIconColor = context.getColor(R.color.default_favicon_background_color);
        mFallbackIconTextSize =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mPendingImageRequests = new HashMap<>();
        mDrawableCache = new HashMap<>();
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
        mDrawableCache.clear();
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
    public void fetchFavicon(GURL url, Callback<@Nullable Drawable> callback) {
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
                    callback.onResult(
                            icon == null
                                    ? null
                                    : new BitmapDrawable(mContext.getResources(), icon));
                });
    }

    /**
     * Asynchronously generate favicon for a given url and deliver the result via supplied callback.
     *
     * @param url The url to generate a favicon for.
     * @param callback The callback that will be invoked with the result.
     */
    public void generateFavicon(GURL url, Callback<@Nullable Drawable> callback) {
        generateFavicon(url, FallbackIconType.ROUNDED_LETTER, callback);
    }

    /**
     * Asynchronously generate favicon for a given url and deliver the result via supplied callback.
     *
     * @param url The url to generate a favicon for.
     * @param fallbackIconType The type of fallback icon to generate if the favicon is not found.
     * @param callback The callback that will be invoked with the result.
     */
    public void generateFavicon(
            GURL url,
            @FallbackIconType int fallbackIconType,
            Callback<@Nullable Drawable> callback) {
        if (!mNativeInitialized) {
            callback.onResult(null);
            return;
        }

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Drawable icon = null;
                    if (fallbackIconType == FallbackIconType.ROUNDED_LETTER) {
                        icon = getOrCreateShape(url);
                    } else if (fallbackIconType == FallbackIconType.GLOBE) {
                        icon = AppCompatResources.getDrawable(mContext, R.drawable.ic_globe_24dp);
                    }
                    callback.onResult(icon);
                });
    }

    /**
     * Returns a generated favicon Drawable for a specific page URL.
     *
     * <p>Created drawables are cached on first creation and reused for efficiency.
     */
    private @Nullable Drawable getOrCreateShape(GURL url) {
        if (GURL.isEmptyOrInvalid(url)) return null;

        String publisher = UrlUtilities.extractPublisherFromPublisherUrl(url);
        if (TextUtils.isEmpty(publisher)) return null;
        String initial = String.valueOf(publisher.charAt(0)).toUpperCase(Locale.getDefault());

        Drawable.ConstantState cachedState = mDrawableCache.get(initial);
        if (cachedState != null) {
            return cachedState.newDrawable();
        }

        Drawable drawable = generateFaviconShape(initial);
        if (drawable != null) {
            mDrawableCache.put(initial, drawable.getConstantState());
        }
        return drawable;
    }

    /** Returns a generated favicon Drawable for a specific website initial. */
    private @Nullable Drawable generateFaviconShape(String initial) {
        Drawable background = getOrCreateBackgroundDrawable();
        if (background == null) return null;

        Path path = generatePathForLetter(initial);
        if (path == null) return null;

        PathShape pathShape = new PathShape(path, mFallbackIconSize, mFallbackIconSize);
        ShapeDrawable textDrawable = new ShapeDrawable(pathShape);
        textDrawable.getPaint().setColor(Color.WHITE);
        textDrawable.setIntrinsicWidth(mFallbackIconSize);
        textDrawable.setIntrinsicHeight(mFallbackIconSize);

        return new LayerDrawable(new Drawable[] {background, textDrawable});
    }

    /**
     * Returns an oval background used by the fallback icon.
     *
     * <p>The shape is created if not already present, then cached and reused with all other
     * fallback drawables.
     */
    private @Nullable Drawable getOrCreateBackgroundDrawable() {
        if (mBackgroundDrawableState == null) {
            ShapeDrawable background = new ShapeDrawable(new OvalShape());
            background.getPaint().setColor(mFallbackIconColor);
            background.setIntrinsicWidth(mFallbackIconSize);
            background.setIntrinsicHeight(mFallbackIconSize);
            mBackgroundDrawableState = background.getConstantState();
        }
        return mBackgroundDrawableState != null ? mBackgroundDrawableState.newDrawable() : null;
    }

    /**
     * Returns a vector path representing a specific latin letter.
     *
     * <p>Letter paths are always created upon request.
     */
    private @Nullable Path generatePathForLetter(String letter) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setTextSize(mFallbackIconTextSize);
        paint.setFakeBoldText(true);

        Path path = new Path();
        float[] widths = new float[1];
        paint.getTextWidths(letter, widths);
        float textWidth = widths[0];
        Paint.FontMetrics fontMetrics = paint.getFontMetrics();
        float fontMaxHeight = fontMetrics.bottom - fontMetrics.top;

        float stdSize = mFallbackIconSize;
        float x = (stdSize - textWidth) / 2f;
        float y = (stdSize - fontMaxHeight) / 2f - fontMetrics.top;

        try {
            paint.getTextPath(letter, 0, letter.length(), x, y, path);
        } catch (Exception e) {
            return null;
        }
        return path;
    }

    /** Clear all cached entries. */
    public void resetCache() {
        if (mIconBridge != null) mIconBridge.createCache(MAX_IMAGE_CACHE_SIZE);
        if (mImageFetcher != null) mImageFetcher.clear();
        mPendingImageRequests.clear();
        // Intentionally not re-setting vector shape cache.
    }

    /**
     * Asynchronously retrieve image for supplied GURL. Calls to this method result with callback
     * being invoked if and only if the fetch was executed and was successful.
     *
     * @param url The url to retrieve a favicon for.
     * @param callback The callback that will be invoked with the result.
     */
    public void fetchImage(GURL url, Callback<Drawable> callback) {
        if (mImageFetcher == null || GURL.isEmptyOrInvalid(url)) {
            return;
        }

        // Do not make duplicate answer image requests for the same URL (to avoid generating
        // duplicate bitmaps for the same image).
        if (mPendingImageRequests.containsKey(url)) {
            mPendingImageRequests.get(url).add(callback);
            return;
        }

        var callbacks = new ArrayList<Callback<Drawable>>();
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

                    Drawable drawable = new BitmapDrawable(mContext.getResources(), bitmap);
                    for (int i = 0; i < pendingCallbacks.size(); i++) {
                        pendingCallbacks.get(i).onResult(drawable);
                    }
                });
    }

    /** Overrides ImageFetcher instance for testing. */
    void setImageFetcherForTesting(@Nullable ImageFetcher fetcher) {
        mImageFetcher = fetcher;
    }
}
