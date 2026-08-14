// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel.dev;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Implements a window-scoped {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelWindowScopedDevFeatureImpl
        implements SidePanelDevFeature, ChromeAndroidTaskFeature {
    private static final String DEV_FEATURE_URL = "https://www.google.com";

    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;

    private @Nullable SidePanelDevFeatureContent mDevContent;
    private long mNativeSidePanelWindowScopedDevFeature;

    private static SidePanelDevFeatureContent createDevContent(
            Profile profile, WindowAndroid windowAndroid) {
        var webContents =
                WebContentsFactory.createWebContents(
                        profile, /* initiallyHidden= */ false, /* initializeRenderer= */ true);
        ContentView contentView =
                ContentView.createContentView(getContext(windowAndroid), webContents);
        webContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());
        webContents.getNavigationController().loadUrl(new LoadUrlParams(DEV_FEATURE_URL));

        var intentRequestTracker = windowAndroid.getIntentRequestTracker();
        assert intentRequestTracker != null;

        // Note:
        //
        // We can't use the WindowAndroid passed into this method to create the ThinWebView as that
        // WindowAndroid already has a compositor. If we reuse that WindowAndroid, ThinWebView
        // creation will fail due to "DCHECK failed: !root_window->GetLayer()" in
        // compositor_impl_android.cc, meaning the ThinWebView can't attach a compositor to a
        // WindowAndroid that already has one.
        //
        // By passing (reusing) the WindowAndroid's IntentRequestTracker, ThinWebView will create
        // its own WindowAndroid using that IntentRequestTracker.
        var thinWebView =
                ThinWebViewFactory.create(
                        getContext(windowAndroid),
                        new ThinWebViewConstraints(),
                        intentRequestTracker,
                        /* enablePermissionRequests= */ false);
        thinWebView.attachWebContents(
                webContents,
                contentView,
                new ThinWebViewAttachParams.Builder()
                        .setWebContentsDelegate(new WebContentsDelegateAndroid())
                        .build());

        return new SidePanelDevFeatureContent(thinWebView, webContents);
    }

    private static Context getContext(WindowAndroid windowAndroid) {
        var context = windowAndroid.getContext().get();
        assert context != null;
        return context;
    }

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
        if (mDevContent != null) {
            mDevContent.destroy();
            mDevContent = null;
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
    private @Nullable View getOrCreateView() {
        if (mDevContent == null) {
            mDevContent = createDevContent(mProfile, mWindowAndroid);
        }

        return assumeNonNull(mDevContent.mThinWebView).getView();
    }

    @NativeMethods
    interface Natives {
        long init(SidePanelWindowScopedDevFeatureImpl caller, long nativeBrowserWindowPtr);

        void destroy(long nativeSidePanelWindowScopedDevFeature);

        void toggle(long nativeSidePanelWindowScopedDevFeature);
    }
}
