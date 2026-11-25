// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.side_panel;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.KeyboardShortcutsDelegate;

/**
 * Manages the Java-side representation and composition of an extension side panel's WebContents.
 *
 * <p>This class wraps the extension's {@link WebContents} in a {@link ThinWebView} to attach it to
 * the Android view hierarchy and compositor surface.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionSidePanelContents implements Destroyable {
    private static boolean sSkipViewEventSinkForTesting;

    private final WebContents mWebContents;
    private final WindowAndroid mWindowAndroid;
    private final ThinWebView mThinWebView;

    public static void setSkipViewEventSinkForTesting(boolean skip) {
        sSkipViewEventSinkForTesting = skip;
        ResettersForTesting.register(() -> sSkipViewEventSinkForTesting = false);
    }

    @CalledByNative
    public static @Nullable ExtensionSidePanelContents create(
            @JniType("content::WebContents*") WebContents webContents,
            WindowAndroid windowAndroid) {
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return null;
        }

        IntentRequestTracker intentRequestTracker = windowAndroid.getIntentRequestTracker();
        if (intentRequestTracker == null) {
            return null;
        }

        ContentView contentView = ContentView.createContentView(context, webContents);

        ViewAndroidDelegate existingDelegate = webContents.getViewAndroidDelegate();
        if (existingDelegate == null) {
            webContents.setDelegates(
                    VersionInfo.getProductVersion(),
                    ViewAndroidDelegate.createBasicDelegate(contentView),
                    contentView,
                    windowAndroid,
                    WebContents.createDefaultInternalsHolder());
        } else {
            webContents.setTopLevelNativeWindow(windowAndroid);
            existingDelegate.setContainerView(contentView);
            if (!sSkipViewEventSinkForTesting) {
                ViewEventSink.from(webContents).setAccessDelegate(contentView);
            }
        }

        ThinWebView thinWebView =
                ThinWebViewFactory.create(
                        context,
                        new ThinWebViewConstraints(),
                        intentRequestTracker,
                        /* enablePermissionRequests= */ false);
        thinWebView.attachWebContents(
                webContents, contentView, new ThinWebViewAttachParams.Builder().build());

        return new ExtensionSidePanelContents(webContents, windowAndroid, thinWebView);
    }

    private ExtensionSidePanelContents(
            WebContents webContents, WindowAndroid windowAndroid, ThinWebView thinWebView) {
        mWebContents = webContents;
        mWindowAndroid = windowAndroid;
        mThinWebView = thinWebView;
    }

    @CalledByNative
    public View getView() {
        return mThinWebView.getView();
    }

    /** Forwards an unhandled renderer key event to the host window's shortcut delegate. */
    @CalledByNative
    boolean handleKeyboardEvent(KeyEvent event) {
        KeyboardShortcutsDelegate delegate = mWindowAndroid.getKeyboardShortcutsDelegate();
        return delegate != null && delegate.handleKeyboardEvent(event);
    }

    @Override
    @CalledByNative
    public void destroy() {
        mThinWebView.destroy();
        if (!mWebContents.isDestroyed()) {
            mWebContents.setTopLevelNativeWindow(null);
            ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
            if (viewDelegate != null) {
                viewDelegate.setContainerView(null);
            }
        }
    }

    public WebContents getWebContents() {
        return mWebContents;
    }
}
