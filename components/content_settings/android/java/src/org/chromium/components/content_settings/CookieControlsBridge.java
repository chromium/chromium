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

/**
 * Communicates between CookieControlsController (C++ backend) and PageInfoView (Java UI).
 */
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
    public CookieControlsBridge(CookieControlsObserver observer, WebContents webContents,
            @Nullable BrowserContextHandle originalBrowserContext) {
        mObserver = observer;
        mNativeCookieControlsBridge = CookieControlsBridgeJni.get().init(
                CookieControlsBridge.this, webContents, originalBrowserContext);
    }

    public void updateWebContents(
            WebContents webContents, @Nullable BrowserContextHandle originalBrowserContext) {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().updateWebContents(
                    mNativeCookieControlsBridge, webContents, originalBrowserContext);
        }
    }

    public void setThirdPartyCookieBlockingEnabledForSite(boolean blockCookies) {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().setThirdPartyCookieBlockingEnabledForSite(
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

    /**
     * Destroys the native counterpart of this class.
     */
    public void destroy() {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().destroy(
                    mNativeCookieControlsBridge, CookieControlsBridge.this);
            mNativeCookieControlsBridge = 0;
        }
    }

    public static boolean isCookieControlsEnabled(BrowserContextHandle handle) {
        return CookieControlsBridgeJni.get().isCookieControlsEnabled(handle);
    }

    public @CookieControlsStatus int getCookieControlsStatus() {
        if (mNativeCookieControlsBridge != 0) {
            return CookieControlsBridgeJni.get().getCookieControlsStatus(
                    mNativeCookieControlsBridge);
        }
        return CookieControlsStatus.UNINITIALIZED;
    }

    public @CookieControlsBreakageConfidenceLevel int getBreakageConfidenceLevel() {
        if (mNativeCookieControlsBridge != 0) {
            return CookieControlsBridgeJni.get().getBreakageConfidenceLevel(
                    mNativeCookieControlsBridge);
        }
        return CookieControlsBreakageConfidenceLevel.UNINITIALIZED;
    }

    @CalledByNative
    private void onCookieBlockingStatusChanged(
            @CookieControlsStatus int status, @CookieControlsEnforcement int enforcement) {
        mObserver.onCookieBlockingStatusChanged(status, enforcement);
    }

    @CalledByNative
    private void onCookiesCountChanged(int allowedCookies, int blockedCookies) {
        mObserver.onCookiesCountChanged(allowedCookies, blockedCookies);
    }

    @CalledByNative
    private void onStatusChanged(
            @CookieControlsStatus int status,
            @CookieControlsEnforcement int enforcement,
            @CookieBlocking3pcdStatus int blockingStatus,
            long expiration) {
        mObserver.onStatusChanged(status, enforcement, blockingStatus, expiration);
    }

    @CalledByNative
    private void onSitesCountChanged(int allowedSites, int blockedSites) {
        mObserver.onSitesCountChanged(allowedSites, blockedSites);
    }

    @CalledByNative
    private void onBreakageConfidenceLevelChanged(
            @CookieControlsBreakageConfidenceLevel int level) {
        mObserver.onBreakageConfidenceLevelChanged(level);
    }

    @NativeMethods
    public interface Natives {
        long init(CookieControlsBridge caller, WebContents webContents,
                BrowserContextHandle originalContextHandle);
        void updateWebContents(long nativeCookieControlsBridge, WebContents webContents,
                @Nullable BrowserContextHandle originalBrowserContext);
        void destroy(long nativeCookieControlsBridge, CookieControlsBridge caller);
        void setThirdPartyCookieBlockingEnabledForSite(
                long nativeCookieControlsBridge, boolean blockCookies);
        void onUiClosing(long nativeCookieControlsBridge);
        void onEntryPointAnimated(long nativeCookieControlsBridge);
        boolean isCookieControlsEnabled(BrowserContextHandle browserContextHandle);
        int getCookieControlsStatus(long nativeCookieControlsBridge);
        int getBreakageConfidenceLevel(long nativeCookieControlsBridge);
    }
}
