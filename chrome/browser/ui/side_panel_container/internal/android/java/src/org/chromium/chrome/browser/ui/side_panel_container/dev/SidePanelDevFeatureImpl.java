// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Implements {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelDevFeatureImpl implements SidePanelDevFeature {
    private static final String DEV_FEATURE_URL = "https://www.google.com";

    private final SidePanelContainerCoordinator mSidePanelContainerCoordinator;
    private @Nullable SidePanelDevFeatureContent mDevContent;

    private static SidePanelDevFeatureContent createDevContent(
            MonotonicObservableSupplier<Profile> profileSupplier, WindowAndroid windowAndroid) {
        Profile profile = profileSupplier.get();
        assert profile != null;

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

    public SidePanelDevFeatureImpl(
            MonotonicObservableSupplier<Profile> profileSupplier,
            SidePanelContainerCoordinator sidePanelContainerCoordinator,
            WindowAndroid windowAndroid) {
        assert ChromeFeatureList.sEnableAndroidSidePanelDevFeature.isEnabled()
                : "SidePanelDevFeature can only be used when its feature flag is enabled";

        mSidePanelContainerCoordinator = sidePanelContainerCoordinator;
        mDevContent = createDevContent(profileSupplier, windowAndroid);
    }

    @Override
    public void toggle() {
        ThreadUtils.assertOnUiThread();
        assert mDevContent != null : "null feature content; is the feature already destroyed?";
        var sidePanelContent = mDevContent.mSidePanelContent;
        assert sidePanelContent != null;

        if (!mSidePanelContainerCoordinator.isShowing(sidePanelContent)) {
            mSidePanelContainerCoordinator.populateContent(
                    sidePanelContent, result -> {}, /* startingBounds= */ null);
        } else {
            mSidePanelContainerCoordinator.removeContentAndClose(
                    result -> {}, /* suppressAnimations= */ true);
        }
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();

        if (mDevContent != null) {
            mDevContent.destroy();
            mDevContent = null;
        }
    }

    @Nullable SidePanelDevFeatureContent getDevFeatureContentForTesting() {
        ThreadUtils.assertOnUiThread();
        return mDevContent;
    }
}
