// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel.test;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Supports {@code side_panel_coordinator_android_browsertest.cc}. */
@NullMarked
public final class SidePanelCoordinatorAndroidBrowserTestSupport {
    private static final String TEST_URL =
            "data:text/html,<html><body><h1>Test Web Page In Side Panel</h1></body></html>";

    private SidePanelCoordinatorAndroidBrowserTestSupport() {}

    @CalledByNative
    private static View createTestView(
            @JniType("Profile*") Profile profile,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            boolean useThinWebView) {
        return useThinWebView
                ? createThinWebView(profile, windowAndroid)
                : createTextView(windowAndroid);
    }

    @SuppressLint("SetTextI18n")
    private static View createTextView(WindowAndroid windowAndroid) {
        TextView view = new TextView(getContext(windowAndroid));
        view.setText("Test Side Panel View");
        view.setBackgroundColor(Color.GREEN);
        view.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        view.setGravity(Gravity.CENTER);
        return view;
    }

    private static View createThinWebView(Profile profile, WindowAndroid windowAndroid) {
        var context = getContext(windowAndroid);
        WebContents webContents =
                WebContentsFactory.createWebContents(
                        profile, /* initiallyHidden= */ false, /* initializeRenderer= */ true);
        ContentView contentView = ContentView.createContentView(context, webContents);
        webContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());
        webContents.getNavigationController().loadUrl(new LoadUrlParams(TEST_URL));

        var intentRequestTracker = windowAndroid.getIntentRequestTracker();
        assert intentRequestTracker != null;

        ThinWebView thinWebView =
                ThinWebViewFactory.create(
                        context,
                        new ThinWebViewConstraints(),
                        intentRequestTracker,
                        /* enablePermissionRequests= */ false);
        thinWebView.attachWebContents(
                webContents,
                contentView,
                new ThinWebViewAttachParams.Builder()
                        .setWebContentsDelegate(new WebContentsDelegateAndroid())
                        .build());

        return thinWebView.getView();
    }

    private static Context getContext(WindowAndroid windowAndroid) {
        var context = windowAndroid.getContext().get();
        assert context != null : "null context in WindowAndroid";
        return context;
    }
}
