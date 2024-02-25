// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import android.content.Context;
import android.content.Intent;

import androidx.fragment.app.FragmentManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.content_public.browser.WebContents;

/** Provides Test-specific behavior for Media Router. */
@JNINamespace("media_router")
public class TestMediaRouterClient extends MediaRouterClient {
    public TestMediaRouterClient() {}

    @Override
    public Context getContextForRemoting() {
        return ContextUtils.getApplicationContext();
    }

    @Override
    public int getTabId(WebContents webContents) {
        return 1;
    }

    @Override
    public Intent createBringTabToFrontIntent(int tabId) {
        return null;
    }

    @Override
    public void showNotification(MediaNotificationInfo notificationInfo) {}

    @Override
    public int getPresentationNotificationId() {
        return 2;
    }

    @Override
    public int getRemotingNotificationId() {
        return 3;
    }

    @Override
    public FragmentManager getSupportFragmentManager(WebContents initiator) {
        return null;
    }

    @Override
    public void addDeferredTask(Runnable deferredTask) {
        deferredTask.run();
    }

    @CalledByNative
    public static void initialize() {
        if (MediaRouterClient.getInstance() != null) return;

        MediaRouterClient.setInstance(new TestMediaRouterClient());
    }
}
