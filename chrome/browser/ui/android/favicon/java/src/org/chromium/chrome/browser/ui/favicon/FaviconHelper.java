// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.favicon;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.List;

/**
 * This is a helper class to use favicon_service.cc's functionality.
 *
 * You can request a favicon image by web page URL. Note that an instance of
 * this class should be created & used & destroyed (by destroy()) in the same
 * thread due to the C++ base::CancelableTaskTracker class
 * requirement.
 */
public class FaviconHelper {
    private long mNativeFaviconHelper;

    /** Callback interface for getting the result from getLocalFaviconImageForURL method. */
    public interface FaviconImageCallback {
        /**
         * This method will be called when the result favicon is ready.
         *
         * @param image Favicon image.
         * @param iconUrl Favicon image's icon url.
         */
        @CalledByNative("FaviconImageCallback")
        public void onFaviconAvailable(Bitmap image, @JniType("GURL") GURL iconUrl);
    }

    /** Similar to {@link FaviconImageCallback} but with a list of urls used in the image. */
    public interface ComposedFaviconImageCallback {
        /**
         * @param image A composed image that contains some or all of the requested favicons.
         * @param iconUrls An ordered array of the icon urls that were used.
         */
        @CalledByNative("ComposedFaviconImageCallback")
        public void onComposedFaviconAvailable(
                Bitmap image, @JniType("std::vector<GURL>") GURL[] iconUrls);
    }

    /** Helper for generating default favicons and sharing the same icon between multiple views. */
    public static class DefaultFaviconHelper {
        private Bitmap mChromeDarkBitmap;
        private Bitmap mChromeLightBitmap;
        private Bitmap mDefaultDarkBitmap;
        private Bitmap mDefaultLightBitmap;

        private int getResourceId(GURL url) {
            return UrlUtilities.isInternalScheme(url)
                    ? R.drawable.chromelogo16
                    : R.drawable.ic_globe_24dp;
        }

        private Bitmap createBitmap(Context context, int resourceId, boolean useDarkIcon) {
            Resources resources = context.getResources();
            Drawable drawable = AppCompatResources.getDrawable(context, resourceId);
            int faviconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
            Bitmap tintedBitmap =
                    Bitmap.createBitmap(faviconSize, faviconSize, Bitmap.Config.ARGB_8888);
            Canvas c = new Canvas(tintedBitmap);
            drawable.setBounds(0, 0, faviconSize, faviconSize);
            final @ColorInt int tintColor =
                    context.getColor(
                            useDarkIcon
                                    ? R.color.default_icon_color_baseline
                                    : R.color.default_icon_color_light);
            drawable.setTint(tintColor);
            drawable.draw(c);
            return tintedBitmap;
        }

        /**
         * Generate a default favicon bitmap for the given URL.
         * @param context The {@link Context} to fetch the icons and tint.
         * @param url The URL of the page whose icon is being generated.
         * @param useDarkIcon Whether a dark icon should be used.
         * @return The favicon.
         */
        public Bitmap getDefaultFaviconBitmap(Context context, GURL url, boolean useDarkIcon) {
            boolean isInternal = UrlUtilities.isInternalScheme(url);
            Bitmap bitmap =
                    isInternal
                            ? (useDarkIcon ? mChromeDarkBitmap : mChromeLightBitmap)
                            : (useDarkIcon ? mDefaultDarkBitmap : mDefaultLightBitmap);
            if (bitmap != null) return bitmap;
            bitmap = createBitmap(context, getResourceId(url), useDarkIcon);
            if (isInternal && useDarkIcon) {
                mChromeDarkBitmap = bitmap;
            } else if (isInternal) {
                mChromeLightBitmap = bitmap;
            } else if (useDarkIcon) {
                mDefaultDarkBitmap = bitmap;
            } else {
                mDefaultLightBitmap = bitmap;
            }
            return bitmap;
        }

        /**
         * Generate a default favicon drawable for the given URL.
         * @param context The {@link Context} used to fetch the default icons and tint.
         * @param url The URL of the page whose icon is being generated.
         * @param useDarkIcon Whether a dark icon should be used.
         * @return The favicon.
         */
        public Drawable getDefaultFaviconDrawable(Context context, GURL url, boolean useDarkIcon) {
            return new BitmapDrawable(
                    context.getResources(), getDefaultFaviconBitmap(context, url, useDarkIcon));
        }

        /**
         * Gives the favicon for given resource id with current theme.
         * @param context The {@link Context} used to fetch the default icons and tint.
         * @param resourceId The integer that represents the id of the icon.
         * @param useDarkIcon Whether a dark icon should be used.
         * @return The favicon
         */
        public Bitmap getThemifiedBitmap(Context context, int resourceId, boolean useDarkIcon) {
            return createBitmap(context, resourceId, useDarkIcon);
        }

        /** Clears any of the cached default drawables. */
        public void clearCache() {
            mChromeDarkBitmap = null;
            mChromeLightBitmap = null;
            mDefaultDarkBitmap = null;
            mDefaultLightBitmap = null;
        }
    }

    /** Allocate and initialize the C++ side of this class. */
    public FaviconHelper() {
        mNativeFaviconHelper = FaviconHelperJni.get().init();
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        assert mNativeFaviconHelper != 0;
        FaviconHelperJni.get().destroy(mNativeFaviconHelper);
        mNativeFaviconHelper = 0;
    }

    /**
     * Get Favicon bitmap for the requested arguments. Retrieves favicons only for pages the user
     * has visited on the current device.
     * @param profile Profile used for the FaviconService construction.
     * @param pageUrl The target Page URL to get the favicon.
     * @param desiredSizeInPixel The size of the favicon in pixel we want to get.
     * @param faviconImageCallback A method to be called back when the result is available. Note
     *         that this callback is not called if this method returns false.
     * @return True if GetLocalFaviconImageForURL is successfully called.
     */
    public boolean getLocalFaviconImageForURL(
            Profile profile,
            GURL pageUrl,
            int desiredSizeInPixel,
            FaviconImageCallback faviconImageCallback) {
        assert mNativeFaviconHelper != 0;
        return FaviconHelperJni.get()
                .getLocalFaviconImageForURL(
                        mNativeFaviconHelper,
                        profile,
                        pageUrl,
                        desiredSizeInPixel,
                        faviconImageCallback);
    }

    /**
     * Get foreign Favicon bitmap for the requested arguments.
     * @param profile Profile used for the FaviconService construction.
     * @param pageUrl The target Page URL to get the favicon.
     * @param desiredSizeInPixel The size of the favicon in pixel we want to get.
     * @param faviconImageCallback A method to be called back when the result is available. Note
     *         that this callback is not called if this method returns false.
     * @return favicon Bitmap corresponding to the pageUrl.
     */
    public boolean getForeignFaviconImageForURL(
            Profile profile,
            GURL pageUrl,
            int desiredSizeInPixel,
            FaviconImageCallback faviconImageCallback) {
        assert mNativeFaviconHelper != 0;
        return FaviconHelperJni.get()
                .getForeignFaviconImageForURL(
                        mNativeFaviconHelper,
                        profile,
                        pageUrl,
                        desiredSizeInPixel,
                        faviconImageCallback);
    }

    /**
     * Get a composed, up to 4, Favicon bitmap for the requested arguments.
     * @param profile Profile used for the FaviconService construction.
     * @param urls The list of URLs whose favicon are requested to compose. Size should be 2 to 4.
     * @param desiredSizeInPixel The size of the favicon in pixel we want to get.
     * @param composedFaviconImageCallback A method to be called back when the result is available.
     *        Note that this callback is not called if this method returns false.
     * @return True if GetLocalFaviconImageForURL is successfully called.
     */
    public boolean getComposedFaviconImage(
            Profile profile,
            @NonNull List<GURL> urls,
            int desiredSizeInPixel,
            ComposedFaviconImageCallback composedFaviconImageCallback) {
        assert mNativeFaviconHelper != 0;

        if (urls.size() <= 1 || urls.size() > 4) {
            throw new IllegalStateException(
                    "Only able to compose 2 to 4 favicon, but requested " + urls.size());
        }

        return FaviconHelperJni.get()
                .getComposedFaviconImage(
                        mNativeFaviconHelper,
                        profile,
                        urls.toArray(new GURL[0]),
                        desiredSizeInPixel,
                        composedFaviconImageCallback);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        long init();

        void destroy(long nativeFaviconHelper);

        boolean getComposedFaviconImage(
                long nativeFaviconHelper,
                @JniType("Profile*") Profile profile,
                @JniType("std::vector<GURL>") GURL[] urls,
                int desiredSizeInDip,
                ComposedFaviconImageCallback composedFaviconImageCallback);

        boolean getLocalFaviconImageForURL(
                long nativeFaviconHelper,
                @JniType("Profile*") Profile profile,
                @JniType("GURL") GURL pageUrl,
                int desiredSizeInDip,
                FaviconImageCallback faviconImageCallback);

        boolean getForeignFaviconImageForURL(
                long nativeFaviconHelper,
                @JniType("Profile*") Profile profile,
                @JniType("GURL") GURL pageUrl,
                int desiredSizeInDip,
                FaviconImageCallback faviconImageCallback);
    }
}
