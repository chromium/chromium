// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/** Communicates between CookieControlsController (C++ backend) and PageInfoView (Java UI). */
@JNINamespace("content_settings")
public class CookieControlsBridge {
    private long mNativeCookieControlsBridge;
    private CookieControlsObserver mObserver;

    /**
     * Initializes a CookieControlsBridge instance.
     * @param observer An observer to call with updates from the cookie controller.
     * @param webContents The WebContents instance to observe.
     * @param originalBrowserContext The "original" browser context. In Chrome, this corresponds to
     *         the regular profile when webContents is incognito.
     */
    public CookieControlsBridge(
            CookieControlsObserver observer,
            WebContents webContents,
            @Nullable BrowserContextHandle originalBrowserContext) {
        mObserver = observer;
        mNativeCookieControlsBridge =
                CookieControlsBridgeJni.get()
                        .init(CookieControlsBridge.this, webContents, originalBrowserContext);
    }

    public void updateWebContents(
            WebContents webContents, @Nullable BrowserContextHandle originalBrowserContext) {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get()
                    .updateWebContents(
                            mNativeCookieControlsBridge, webContents, originalBrowserContext);
        }
    }

    public void setThirdPartyCookieBlockingEnabledForSite(boolean blockCookies) {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get()
                    .setThirdPartyCookieBlockingEnabledForSite(
                            mNativeCookieControlsBridge, blockCookies);
        }
    }

    public void onUiClosing() {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().onUiClosing(mNativeCookieControlsBridge);
        }
    }

    public void onEntryPointAnimated() {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().onEntryPointAnimated(mNativeCookieControlsBridge);
        }
    }

    /** Destroys the native counterpart of this class. */
    public void destroy() {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get()
                    .destroy(mNativeCookieControlsBridge, CookieControlsBridge.this);
            mNativeCookieControlsBridge = 0;
        }
    }

    public static boolean isCookieControlsEnabled(BrowserContextHandle handle) {
        return CookieControlsBridgeJni.get().isCookieControlsEnabled(handle);
    }

    /** Container for the struct defined in tracking_protection_feature.h on the C++ side. */
    public static class TrackingProtectionFeature {
        // The feature that this struct applies to.
        public @TrackingProtectionFeatureType int featureType;
        // If enforced then how (by policy, setting, etc).
        public @CookieControlsEnforcement int enforcement;
        // The status of the feature (whether it's allowed, blocked, limited, etc).
        public @TrackingProtectionBlockingStatus int status;

        public TrackingProtectionFeature(
                @TrackingProtectionFeatureType int featureType,
                @CookieControlsEnforcement int enforcement,
                @TrackingProtectionBlockingStatus int status) {
            this.featureType = featureType;
            this.enforcement = enforcement;
            this.status = status;
        }
    }

    @CalledByNative
    private static List<TrackingProtectionFeature> createTpFeatureList() {
        return new ArrayList<TrackingProtectionFeature>();
    }

    @CalledByNative
    private static void createTpFeatureAndAddToList(
            List<TrackingProtectionFeature> list,
            @TrackingProtectionFeatureType int featureType,
            @CookieControlsEnforcement int enforcement,
            @TrackingProtectionBlockingStatus int status) {
        TrackingProtectionFeature feature =
                new TrackingProtectionFeature(featureType, enforcement, status);

        if (list != null) list.add(feature);
    }

    @CalledByNative
    private void onStatusChanged(
            boolean controlsVisible,
            boolean protectionsOn,
            @CookieControlsEnforcement int enforcement,
            @CookieBlocking3pcdStatus int blockingStatus,
            long expiration,
            List<TrackingProtectionFeature> features) {
        // Old cookies API.
        mObserver.onStatusChanged(
                controlsVisible, protectionsOn, enforcement, blockingStatus, expiration);
        // New Tracking Protection API.
        mObserver.onTrackingProtectionStatusChanged(
                controlsVisible, protectionsOn, expiration, features);
    }

    @CalledByNative
    private void onHighlightCookieControl(boolean shouldHighlight) {
        mObserver.onHighlightCookieControl(shouldHighlight);
    }

    @CalledByNative
    private void onHighlightPwaCookieControl() {
        mObserver.onHighlightPwaCookieControl();
    }

    @NativeMethods
    public interface Natives {
        long init(
                CookieControlsBridge caller,
                WebContents webContents,
                BrowserContextHandle originalContextHandle);

        void updateWebContents(
                long nativeCookieControlsBridge,
                WebContents webContents,
                @Nullable BrowserContextHandle originalBrowserContext);

        void destroy(long nativeCookieControlsBridge, CookieControlsBridge caller);

        void setThirdPartyCookieBlockingEnabledForSite(
                long nativeCookieControlsBridge, boolean blockCookies);

        void onUiClosing(long nativeCookieControlsBridge);

        void onEntryPointAnimated(long nativeCookieControlsBridge);

        boolean isCookieControlsEnabled(BrowserContextHandle browserContextHandle);
    }
}
