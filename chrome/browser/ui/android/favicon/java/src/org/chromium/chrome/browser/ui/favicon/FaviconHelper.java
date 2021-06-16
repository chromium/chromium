// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.favicon;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
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

    /**
     * Callback interface for getting the result from getLocalFaviconImageForURL method.
     */
    public interface FaviconImageCallback {
        /**
         * This method will be called when the result favicon is ready.
         * @param image Favicon image.
         * @param iconUrl Favicon image's icon url.
         */
        @CalledByNative("FaviconImageCallback")
        public void onFaviconAvailable(Bitmap image, GURL iconUrl);
    }

    /**
     * Helper for generating default favicons and sharing the same icon between multiple views.
     */
    public static class DefaultFaviconHelper {
        private Bitmap mChromeDarkBitmap;
        private Bitmap mChromeLightBitmap;
        private Bitmap mDefaultDarkBitmap;
        private Bitmap mDefaultLightBitmap;

        private int getResourceId(GURL url) {
            return UrlUtilities.isInternalScheme(url) ? R.drawable.chromelogo16
                                                      : R.drawable.default_favicon;
        }

        private Bitmap createBitmap(Resources resources, GURL url, boolean useDarkIcon) {
            Bitmap origBitmap = BitmapFactory.decodeResource(resources, getResourceId(url));
            Bitmap tintedBitmap = Bitmap.createBitmap(
                    origBitmap.getWidth(), origBitmap.getHeight(), Bitmap.Config.ARGB_8888);
            Canvas c = new Canvas(tintedBitmap);
            @ColorInt
            int tintColor = ApiCompatibilityUtils.getColor(resources,
                    useDarkIcon ? R.color.default_icon_color : R.color.default_icon_color_light);
            Paint p = new Paint();
            p.setColorFilter(new PorterDuffColorFilter(tintColor, PorterDuff.Mode.SRC_IN));
            c.drawBitmap(origBitmap, 0f, 0f, p);
            return tintedBitmap;
        }

        /**
         * Generate a default favicon bitmap for the given URL.
         * @param resources The {@link Resources} to fetch the icons.
         * @param url The URL of the page whose icon is being generated.
         * @param useDarkIcon Whether a dark icon should be used.
         * @return The favicon.
         */
        public Bitmap getDefaultFaviconBitmap(Resources resources, GURL url, boolean useDarkIcon) {
            boolean isInternal = UrlUtilities.isInternalScheme(url);
            Bitmap bitmap = isInternal ? (useDarkIcon ? mChromeDarkBitmap : mChromeLightBitmap)
                                       : (useDarkIcon ? mDefaultDarkBitmap : mDefaultLightBitmap);
            if (bitmap != null) return bitmap;
            bitmap = createBitmap(resources, url, useDarkIcon);
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
         * @param resources The {@link Resources} used to fetch the default icons.
         * @param url The URL of the page whose icon is being generated.
         * @param useDarkIcon Whether a dark icon should be used.
         * @return The favicon.
         */
        public Drawable getDefaultFaviconDrawable(
                Resources resources, GURL url, boolean useDarkIcon) {
            return new BitmapDrawable(
                    resources, getDefaultFaviconBitmap(resources, url, useDarkIcon));
        }

        /** Clears any of the cached default drawables. */
        public void clearCache() {
            mChromeDarkBitmap = null;
            mChromeLightBitmap = null;
            mDefaultDarkBitmap = null;
            mDefaultLightBitmap = null;
        }
    }

    /**
     * Allocate and initialize the C++ side of this class.
     */
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
    public boolean getLocalFaviconImageForURL(Profile profile, GURL pageUrl, int desiredSizeInPixel,
            FaviconImageCallback faviconImageCallback) {
        return getLocalFaviconImageForURL(
                profile, pageUrl.getSpec(), desiredSizeInPixel, faviconImageCallback);
    }

    @Deprecated // Use GURL version instead.
    public boolean getLocalFaviconImageForURL(Profile profile, String pageUrl,
            int desiredSizeInPixel, FaviconImageCallback faviconImageCallback) {
        assert mNativeFaviconHelper != 0;
        // TODO(crbug/783819): Convert to GURL.
        return FaviconHelperJni.get().getLocalFaviconImageForURL(
                mNativeFaviconHelper, profile, pageUrl, desiredSizeInPixel, faviconImageCallback);
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
    public boolean getForeignFaviconImageForURL(Profile profile, GURL pageUrl,
            int desiredSizeInPixel, FaviconImageCallback faviconImageCallback) {
        assert mNativeFaviconHelper != 0;
        return FaviconHelperJni.get().getForeignFaviconImageForURL(
                mNativeFaviconHelper, profile, pageUrl, desiredSizeInPixel, faviconImageCallback);
    }

    /**
     * Get a composed, up to 4, Favicon bitmap for the requested arguments.
     * @param profile Profile used for the FaviconService construction.
     * @param urls The list of URLs whose favicon are requested to compose. Size should be 2 to 4.
     * @param desiredSizeInPixel The size of the favicon in pixel we want to get.
     * @param faviconImageCallback A method to be called back when the result is available. Note
     *         that this callback is not called if this method returns false.
     * @return True if GetLocalFaviconImageForURL is successfully called.
     */
    public boolean getComposedFaviconImage(Profile profile, @NonNull List<GURL> urls,
            int desiredSizeInPixel, FaviconImageCallback faviconImageCallback) {
        assert mNativeFaviconHelper != 0;

        if (urls.size() <= 1 || urls.size() > 4) {
            throw new IllegalStateException(
                    "Only able to compose 2 to 4 favicon, but requested " + urls.size());
        }

        return FaviconHelperJni.get().getComposedFaviconImage(mNativeFaviconHelper, profile,
                urls.toArray(new GURL[0]), desiredSizeInPixel, faviconImageCallback);
    }

    @NativeMethods
    interface Natives {
        long init();
        void destroy(long nativeFaviconHelper);
        boolean getComposedFaviconImage(long nativeFaviconHelper, Profile profile, GURL[] urls,
                int desiredSizeInDip, FaviconImageCallback faviconImageCallback);
        boolean getLocalFaviconImageForURL(long nativeFaviconHelper, Profile profile,
                String pageUrl, int desiredSizeInDip, FaviconImageCallback faviconImageCallback);
        boolean getForeignFaviconImageForURL(long nativeFaviconHelper, Profile profile,
                GURL pageUrl, int desiredSizeInDip, FaviconImageCallback faviconImageCallback);
    }
}
