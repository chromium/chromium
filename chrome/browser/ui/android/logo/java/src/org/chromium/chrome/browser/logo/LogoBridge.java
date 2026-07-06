// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Provides access to the search provider's logo via the C++ LogoService. */
@NullMarked
public class LogoBridge {
    /** A logo for a search provider (e.g. the Yahoo! logo or Google doodle). */
    public static class Logo {
        /** The logo image. Non-null. */
        public final Bitmap image;

        /** The dark mode logo image. May be null. */
        public final @Nullable Bitmap darkImage;

        /** The URL to navigate to when the user clicks on the logo. */
        public final @Nullable String onClickUrl;

        /** The accessibility text describing the logo. */
        public final @Nullable String altText;

        /**
         * The URL to download animated GIF logo. If null, there is no animated logo to download.
         */
        public final @Nullable String animatedLogoUrl;

        /** The URL to ping when the logo is shown. */
        public final @Nullable String logUrl;

        /**
         * The URL to download dark mode animated GIF logo. If null, there is no dark animated logo
         * to download.
         */
        public final @Nullable String darkAnimatedLogoUrl;

        @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
        public Logo(
                Bitmap image,
                @Nullable Bitmap darkImage,
                @Nullable String onClickUrl,
                @Nullable String altText,
                @Nullable String animatedLogoUrl,
                @Nullable String darkAnimatedLogoUrl,
                @Nullable String logUrl) {
            this.image = image;
            this.darkImage = darkImage;
            this.onClickUrl = onClickUrl;
            this.altText = altText;
            this.animatedLogoUrl = animatedLogoUrl;
            this.darkAnimatedLogoUrl = darkAnimatedLogoUrl;
            this.logUrl = logUrl;
        }
    }

    /** Observer for receiving the logo when it's available. */
    public interface LogoObserver {
        /**
         * Called when the cached or fresh logo is available. This may be called up to two times,
         * once with the cached logo and once with a freshly downloaded logo.
         *
         * @param logo The search provider's logo.
         * @param fromCache Whether the logo was loaded from the cache.
         */
        @CalledByNative
        void onLogoAvailable(Logo logo, boolean fromCache);
    }

    private long mNativeLogoBridge;

    /**
     * Creates a LogoBridge for getting the logo of the default search provider.
     *
     * @param profile Profile of the tab that will show the logo.
     */
    public LogoBridge(Profile profile) {
        mNativeLogoBridge = LogoBridgeJni.get().init(profile);
    }

    /**
     * Cleans up the C++ side of this class. After calling this, LogoObservers passed to
     * getCurrentLogo() will no longer receive updates.
     */
    void destroy() {
        assert mNativeLogoBridge != 0;
        LogoBridgeJni.get().destroy(mNativeLogoBridge);
        mNativeLogoBridge = 0;
    }

    /**
     * Gets the current logo for the default search provider.
     *
     * @param logoObserver The observer to receive the cached and/or fresh logos when they're
     *     available. logoObserver.onLogoAvailable() may be called synchronously if the cached logo
     *     is already available.
     */
    void getCurrentLogo(LogoObserver logoObserver) {
        LogoBridgeJni.get().getCurrentLogo(mNativeLogoBridge, logoObserver);
    }

    /**
     * Records an impression for a doodle.
     *
     * @param logUrl The URL to ping to record the impression.
     */
    public void recordImpression(@Nullable String logUrl) {
        if (mNativeLogoBridge != 0 && logUrl != null) {
            LogoBridgeJni.get().recordImpression(mNativeLogoBridge, logUrl);
        }
    }

    @CalledByNative
    private static Logo createLogo(
            Bitmap image,
            @Nullable Bitmap darkImage,
            @Nullable String onClickUrl,
            @Nullable String altText,
            @Nullable String gifUrl,
            @Nullable String darkGifUrl,
            @Nullable String logUrl) {
        return new Logo(image, darkImage, onClickUrl, altText, gifUrl, darkGifUrl, logUrl);
    }

    @NativeMethods
    public interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void getCurrentLogo(long nativeLogoBridge, LogoObserver logoObserver);

        void recordImpression(long nativeLogoBridge, @JniType("std::string") String logUrl);

        void destroy(long nativeLogoBridge);
    }
}
