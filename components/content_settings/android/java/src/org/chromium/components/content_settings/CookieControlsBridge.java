// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/** Communicates between CookieControlsController (C++ backend) and PageInfoView (Java UI). */
@JNINamespace("content_settings")
@NullMarked
public class CookieControlsBridge {
    private long mNativeCookieControlsBridge;
    private final CookieControlsObserver mObserver;

    /**
     * Initializes a CookieControlsBridge instance.
     *
     * @param observer An observer to call with updates from the cookie controller.
     * @param webContents The WebContents instance to observe.
     * @param originalBrowserContext The "original" browser context. In Chrome, this corresponds to
     *     the regular profile when the current profile is off the record.
     * @param isIncognitoBranded boolean that determines whether the profile is incognito.
     */
    public CookieControlsBridge(
            CookieControlsObserver observer,
            WebContents webContents,
            @Nullable BrowserContextHandle originalBrowserContext,
            boolean isIncognitoBranded) {
        mObserver = observer;
        mNativeCookieControlsBridge =
                CookieControlsBridgeJni.get()
                        .init(this, webContents, originalBrowserContext, isIncognitoBranded);
    }

    /**
     * Called when web contents have changed.
     *
     * @param webContents The WebContents instance to update to.
     * @param originalBrowserContext The "original" browser context. In Chrome, this corresponds to
     *     the regular profile when the current profile is off the record.
     * @param isIncognitoBranded boolean that determines whether the profile is incognito.
     */
    public void updateWebContents(
            WebContents webContents,
            @Nullable BrowserContextHandle originalBrowserContext,
            boolean isIncognitoBranded) {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get()
                    .updateWebContents(
                            mNativeCookieControlsBridge,
                            webContents,
                            originalBrowserContext,
                            isIncognitoBranded);
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
            CookieControlsBridgeJni.get().destroy(mNativeCookieControlsBridge);
            mNativeCookieControlsBridge = 0;
        }
    }

    public static boolean isCookieControlsEnabled(BrowserContextHandle handle) {
        return CookieControlsBridgeJni.get().isCookieControlsEnabled(handle);
    }

    @CalledByNative
    private void onStatusChanged(
            @CookieControlsState int controlsState,
            @CookieControlsEnforcement int enforcement,
            @CookieBlocking3pcdStatus int blockingStatus,
            long expiration) {
        mObserver.onStatusChanged(controlsState, enforcement, blockingStatus, expiration);
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
                CookieControlsBridge self,
                WebContents webContents,
                @Nullable BrowserContextHandle originalContextHandle,
                boolean isIncognitoBranded);

        void updateWebContents(
                long nativeCookieControlsBridge,
                WebContents webContents,
                @Nullable BrowserContextHandle originalBrowserContext,
                boolean isIncognitoBranded);

        void destroy(long nativeCookieControlsBridge);

        void setThirdPartyCookieBlockingEnabledForSite(
                long nativeCookieControlsBridge, boolean blockCookies);

        void onUiClosing(long nativeCookieControlsBridge);

        void onEntryPointAnimated(long nativeCookieControlsBridge);

        boolean isCookieControlsEnabled(BrowserContextHandle browserContextHandle);
    }
}
