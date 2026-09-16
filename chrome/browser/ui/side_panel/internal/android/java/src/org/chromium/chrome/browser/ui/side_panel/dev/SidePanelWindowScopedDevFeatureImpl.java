// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel.dev;

import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.thin_webview.ThinWebViewHost;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Implements a window-scoped {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelWindowScopedDevFeatureImpl
        implements SidePanelDevFeature, ChromeAndroidTaskFeature {
    private static final String DEV_FEATURE_URL = "https://www.google.com";

    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;

    private @Nullable ThinWebViewHost mHost;
    private @Nullable WebContents mWebContents;
    private long mNativeSidePanelWindowScopedDevFeature;

    public SidePanelWindowScopedDevFeatureImpl(Profile profile, WindowAndroid windowAndroid) {
        assert AndroidSidePanelEnabledFn.isWindowScopedDevFeatureEnabled();

        mProfile = profile;
        mWindowAndroid = windowAndroid;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of ChromeAndroidTaskFeature Implementation                             //
    ///////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    public void onAddedToTask(InitInfo initInfo) {
        ThreadUtils.assertOnUiThread();
        createNativePtr(initInfo.nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        ThreadUtils.assertOnUiThread();
        if (mHost != null) {
            mHost.destroy();
            mHost = null;
        }
        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
        }

        destroyNativePtr();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of ChromeAndroidTaskFeature Implementation                             //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start of SidePanelDevFeature Implementation                                  //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public void toggle() {
        ThreadUtils.assertOnUiThread();
        if (mNativeSidePanelWindowScopedDevFeature != 0) {
            SidePanelWindowScopedDevFeatureImplJni.get()
                    .toggle(mNativeSidePanelWindowScopedDevFeature);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              End of SidePanelDevFeature Implementation                                    //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    private void createNativePtr(long nativeBrowserWindowPtr) {
        assert nativeBrowserWindowPtr != 0
                : "Native BrowserWindowInterface pointer shouldn't be null.";
        assert mNativeSidePanelWindowScopedDevFeature == 0
                : "Native SidePanelWindowScopedDevFeature already exists";

        mNativeSidePanelWindowScopedDevFeature =
                SidePanelWindowScopedDevFeatureImplJni.get().init(this, nativeBrowserWindowPtr);
    }

    private void destroyNativePtr() {
        if (mNativeSidePanelWindowScopedDevFeature != 0) {
            SidePanelWindowScopedDevFeatureImplJni.get()
                    .destroy(mNativeSidePanelWindowScopedDevFeature);
            mNativeSidePanelWindowScopedDevFeature = 0;
        }
    }

    @CalledByNative
    private View getOrCreateView() {
        if (mHost == null) {
            var webContents =
                    WebContentsFactory.createWebContents(
                            mProfile, /* initiallyHidden= */ false, /* initializeRenderer= */ true);
            webContents.getNavigationController().loadUrl(new LoadUrlParams(DEV_FEATURE_URL));
            mWebContents = webContents;
            mHost = ThinWebViewHost.create(webContents, mWindowAndroid);
        }

        return mHost.getView();
    }

    @NativeMethods
    interface Natives {
        long init(SidePanelWindowScopedDevFeatureImpl caller, long nativeBrowserWindowPtr);

        void destroy(long nativeSidePanelWindowScopedDevFeature);

        void toggle(long nativeSidePanelWindowScopedDevFeature);
    }
}
