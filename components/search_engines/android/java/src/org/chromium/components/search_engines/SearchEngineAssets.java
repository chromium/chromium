// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.function.IntFunction;

/** Utility class for retrieving search engine assets (like favicons). */
@NullMarked
public class SearchEngineAssets implements Destroyable {
    /**
     * Represents a retrieved search engine icon, abstracting whether it is backed by a {@link
     * Bitmap} or an Android drawable resource.
     */
    public static final class SearchEngineIcon {
        private final @Nullable Bitmap mBitmap;
        private final @DrawableRes int mDrawableRes;
        private final boolean mIsMonochrome;

        private SearchEngineIcon(
                @Nullable Bitmap bitmap, @DrawableRes int drawableRes, boolean isMonochrome) {
            mBitmap = bitmap;
            mDrawableRes = drawableRes;
            mIsMonochrome = isMonochrome;
        }

        /** Creates a non-monochrome {@link SearchEngineIcon} backed by a {@link Bitmap}. */
        public static SearchEngineIcon fromBitmap(Bitmap bitmap) {
            return new SearchEngineIcon(bitmap, Resources.ID_NULL, /* isMonochrome= */ false);
        }

        /** Creates a {@link SearchEngineIcon} backed by a drawable resource ID. */
        public static SearchEngineIcon fromDrawableRes(
                @DrawableRes int drawableRes, boolean isMonochrome) {
            assert drawableRes != Resources.ID_NULL;
            return new SearchEngineIcon(/* bitmap= */ null, drawableRes, isMonochrome);
        }

        /** Returns whether the icon is monochrome and suitable for tinting by the caller. */
        public boolean isMonochrome() {
            return mIsMonochrome;
        }

        /** Converts this icon to a {@link Drawable} using the supplied {@link Context}. */
        public Drawable toDrawable(Context context) {
            return toDrawable(context, res -> assertNonNull(context.getDrawable(res)));
        }

        /**
         * Converts this icon to a {@link Drawable}, using {@code drawableLoader} to resolve
         * resource-backed icons (e.g., via a drawable cache).
         */
        public Drawable toDrawable(Context context, IntFunction<Drawable> drawableLoader) {
            if (mBitmap != null) {
                return new BitmapDrawable(context.getResources(), mBitmap);
            }
            return drawableLoader.apply(mDrawableRes);
        }

        @Override
        public boolean equals(@Nullable Object other) {
            if (this == other) return true;
            if (!(other instanceof SearchEngineIcon that)) return false;
            return mDrawableRes == that.mDrawableRes
                    && mIsMonochrome == that.mIsMonochrome
                    && Objects.equals(mBitmap, that.mBitmap);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mBitmap, mDrawableRes, mIsMonochrome);
        }
    }

    private static @Nullable Boolean sIsChromeBrandedForTesting;

    private final LargeIconBridge mLargeIconBridge;
    private final Map<String, SearchEngineIcon> mBuiltInIconCache = new HashMap<>();
    private boolean mIsDestroyed;

    public SearchEngineAssets(BrowserContextHandle browserContext) {
        mLargeIconBridge = new LargeIconBridge(browserContext);
    }

    @VisibleForTesting
    public SearchEngineAssets(LargeIconBridge bridge) {
        mLargeIconBridge = bridge;
    }

    @Override
    public void destroy() {
        mIsDestroyed = true;
        mLargeIconBridge.destroy();
        mBuiltInIconCache.clear();
    }

    /**
     * Retrieve the favicon for the given TemplateUrl.
     *
     * @param templateUrlService The TemplateUrlService to check search engine types.
     * @param templateUrl The TemplateUrl to retrieve the favicon for.
     * @param desiredSizePx The desired size of the icon in pixels.
     * @param callback The callback to receive the {@link SearchEngineIcon}, or {@code null} if not
     *     found.
     */
    public void retrieveFavicon(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<@Nullable SearchEngineIcon> callback) {
        retrieveFaviconFromBrandedResources(
                templateUrlService, templateUrl, desiredSizePx, callback);
    }

    private void retrieveFaviconFromBrandedResources(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<@Nullable SearchEngineIcon> callback) {
        // Branded resources are only available on Chrome branded builds.
        if (isChromeBranded()) {
            String faviconUrlSpec = templateUrl.getFaviconURL().getSpec();
            if (!TextUtils.isEmpty(faviconUrlSpec)) {
                SearchEngineIcon cachedIcon = mBuiltInIconCache.get(faviconUrlSpec);
                if (cachedIcon != null) {
                    callback.onResult(cachedIcon);
                    return;
                }
            }

            byte @Nullable [] pngData = templateUrl.getBuiltInSearchEngineIconPngBytes();
            if (pngData != null) {
                decodeBrandedIconAsync(pngData, faviconUrlSpec, callback);
                return;
            }
        }
        retrieveFaviconForStarterPack(templateUrlService, templateUrl, desiredSizePx, callback);
    }

    private void decodeBrandedIconAsync(
            byte[] pngData, String faviconUrlSpec, Callback<@Nullable SearchEngineIcon> callback) {
        PostTask.postTask(
                TaskTraits.USER_VISIBLE,
                () -> {
                    Bitmap bm = BitmapFactory.decodeByteArray(pngData, 0, pngData.length);
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> onBrandedIconDecoded(bm, faviconUrlSpec, callback));
                });
    }

    private void onBrandedIconDecoded(
            @Nullable Bitmap bm,
            String faviconUrlSpec,
            Callback<@Nullable SearchEngineIcon> callback) {
        if (mIsDestroyed) return;
        if (bm == null) {
            callback.onResult(null);
            return;
        }
        SearchEngineIcon icon = SearchEngineIcon.fromBitmap(bm);
        if (!TextUtils.isEmpty(faviconUrlSpec)) {
            mBuiltInIconCache.put(faviconUrlSpec, icon);
        }
        callback.onResult(icon);
    }

    private static boolean isChromeBranded() {
        return sIsChromeBrandedForTesting != null
                ? sIsChromeBrandedForTesting
                : BuildConfig.IS_CHROME_BRANDED;
    }

    private void retrieveFaviconForStarterPack(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<@Nullable SearchEngineIcon> callback) {
        @StarterPackId int starterPackId = templateUrl.getStarterPackId();
        if (starterPackId != StarterPackId.NONE) {
            // TODO(crbug.com/522176230): Apply desiredSizePx to regenerate starter pack icons.
            @DrawableRes int drawableRes = getStarterPackDrawableRes(starterPackId);
            if (drawableRes != Resources.ID_NULL) {
                callback.onResult(
                        SearchEngineIcon.fromDrawableRes(drawableRes, /* isMonochrome= */ true));
                return;
            }
        }
        retrieveFaviconFromDefaultResources(
                templateUrlService, templateUrl, desiredSizePx, callback);
    }

    private void retrieveFaviconFromDefaultResources(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<@Nullable SearchEngineIcon> callback) {
        if (templateUrlService.getSearchEngineTypeFromTemplateUrl(templateUrl.getKeyword())
                != SearchEngineType.SEARCH_ENGINE_GOOGLE) {
            // Fall back to next source.
            retrieveFaviconFromOriginUrl(templateUrl, desiredSizePx, callback);
            return;
        }
        callback.onResult(
                SearchEngineIcon.fromDrawableRes(
                        R.drawable.ic_logo_googleg_20dp, /* isMonochrome= */ false));
    }

    private void retrieveFaviconFromOriginUrl(
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<@Nullable SearchEngineIcon> callback) {
        var originUrl = new GURL(templateUrl.getURL()).getOrigin();
        boolean willCall =
                mLargeIconBridge.getLargeIconForUrl(
                        originUrl,
                        /* minSizePx= */ 1,
                        desiredSizePx,
                        (image, fallbackColor, isFallbackColorDefault, iconType) -> {
                            callback.onResult(
                                    image == null ? null : SearchEngineIcon.fromBitmap(image));
                        });
        if (!willCall) {
            callback.onResult(null);
        }
    }

    /**
     * Returns the drawable resource ID for the given starter pack ID, or {@link Resources#ID_NULL}
     * if no dedicated starter pack icon exists.
     */
    public static @DrawableRes int getStarterPackDrawableRes(@StarterPackId int starterPackId) {
        return switch (starterPackId) {
            case StarterPackId.BOOKMARKS -> R.drawable.ic_star_24dp;
            case StarterPackId.HISTORY -> R.drawable.ic_history_24dp;
            case StarterPackId.TABS -> R.drawable.switch_to_tab;
            case StarterPackId.GEMINI -> R.drawable.ic_spark_24dp;
            case StarterPackId.AI_MODE -> R.drawable.search_spark;
            default -> Resources.ID_NULL;
        };
    }

    /** Returns the monochrome default fallback search engine {@link SearchEngineIcon}. */
    public static SearchEngineIcon getDefaultFavicon() {
        return SearchEngineIcon.fromDrawableRes(
                R.drawable.ic_search_24dp, /* isMonochrome= */ true);
    }

    /** Overrides {@link BuildConfig#IS_CHROME_BRANDED} for testing. */
    public static void setIsChromeBrandedForTesting(boolean isChromeBranded) {
        sIsChromeBrandedForTesting = isChromeBranded;
        ResettersForTesting.register(() -> sIsChromeBrandedForTesting = null);
    }
}
