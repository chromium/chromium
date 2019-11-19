// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.os.IBinder;
import android.support.annotation.ColorInt;
import android.support.annotation.Nullable;
import android.widget.FrameLayout;

import org.chromium.chromecast.base.Observer;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

class CastWebContentsScopes {
    interface WindowTokenProvider {
        @Nullable
        IBinder provideWindowToken();
    }

    static final String VIEW_TAG_CONTENT_VIEW = "ContentView";

    public static Observer<WebContents> onLayoutActivity(
            Activity activity, FrameLayout layout, @ColorInt int backgroundColor) {
        layout.setBackgroundColor(backgroundColor);
        WindowAndroid window = new ActivityWindowAndroid(activity);
        return onLayoutInternal(activity, layout, window, backgroundColor);
    }

    public static Observer<WebContents> onLayoutFragment(
            Activity activity, FrameLayout layout, @ColorInt int backgroundColor) {
        layout.setBackgroundColor(backgroundColor);
        WindowAndroid window = new WindowAndroid(activity);
        return onLayoutInternal(activity, layout, window, backgroundColor);
    }

    static Observer<WebContents> onLayoutView(Context context, FrameLayout layout,
            @ColorInt int backgroundColor, WindowTokenProvider windowTokenProvider) {
        layout.setBackgroundColor(backgroundColor);
        WindowAndroid window = new WindowAndroid(context) {
            @Override
            protected IBinder getWindowToken() {
                return windowTokenProvider.provideWindowToken();
            }
        };
        return onLayoutInternal(context, layout, window, backgroundColor);
    }

    private static Observer<WebContents> onLayoutInternal(Context context, FrameLayout layout,
            WindowAndroid window, @ColorInt int backgroundColor) {
        return (WebContents webContents) -> {
            ContentViewRenderView contentViewRenderView = new ContentViewRenderView(context) {
                @Override
                protected void onReadyToRender() {
                    setOverlayVideoMode(true);
                }
            };
            contentViewRenderView.onNativeLibraryLoaded(window);
            contentViewRenderView.setSurfaceViewBackgroundColor(backgroundColor);
            FrameLayout.LayoutParams matchParent = new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
            layout.addView(contentViewRenderView, matchParent);

            ContentView contentView = ContentView.createContentView(context, webContents);
            // TODO(derekjchow): productVersion
            webContents.initialize("", ViewAndroidDelegate.createBasicDelegate(contentView),
                    contentView, window, WebContents.createDefaultInternalsHolder());

            // Enable display of current webContents.
            webContents.onShow();
            layout.addView(contentView, matchParent);
            contentView.setFocusable(true);
            contentView.requestFocus();
            contentView.setTag(VIEW_TAG_CONTENT_VIEW);
            contentViewRenderView.setCurrentWebContents(webContents);
            return () -> {
                layout.setForeground(new ColorDrawable(backgroundColor));
                layout.removeView(contentView);
                layout.removeView(contentViewRenderView);
                contentViewRenderView.destroy();
                window.destroy();
            };
        };
    }

    public static Observer<WebContents> withoutLayout(Context context) {
        return (WebContents webContents) -> {
            WindowAndroid window = new WindowAndroid(context);
            ContentView contentView = ContentView.createContentView(context, webContents);
            // TODO(derekjchow): productVersion
            webContents.initialize("", ViewAndroidDelegate.createBasicDelegate(contentView),
                    contentView, window, WebContents.createDefaultInternalsHolder());
            // Enable display of current webContents.
            webContents.onShow();
            return () -> {
                if (!webContents.isDestroyed()) {
                    // WebContents can be destroyed by the app before CastWebContentsComponent
                    // unbinds, which is why we need this check.
                    webContents.onHide();
                }
            };
        };
    }
}
