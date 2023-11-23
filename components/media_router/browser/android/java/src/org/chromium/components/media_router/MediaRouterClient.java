// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;

import androidx.fragment.app.FragmentManager;

import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.content_public.browser.WebContents;

/** An abstraction that allows embedders to implement behavior needed by shared Media Router code. */
public abstract class MediaRouterClient {
    @SuppressLint("StaticFieldLeak")
    private static MediaRouterClient sInstance;

    /**
     * Sets the singleton client instance.
     * @param client the {@link Client} provided by the given embedder.
     */
    public static void setInstance(MediaRouterClient mediaRouterClient) {
        sInstance = mediaRouterClient;
    }

    public static MediaRouterClient getInstance() {
        return sInstance;
    }

    /**
     * Returns a context that can be passed to {@link CastContext}.
     *
     * The value that {@link getApplicationContext()} returns for this context must be an {@link
     * Application}.
     */
    public abstract Context getContextForRemoting();

    /**
     * @param webContents a {@link WebContents} in a tab.
     * @return a unique integer identifier for the associated tab.
     */
    public abstract int getTabId(WebContents webContents);

    /**
     * @param tabId a tab identifier.
     * @return an {@link Intent} that brings the identified tab to the foreground.
     */
    public abstract Intent createBringTabToFrontIntent(int tabId);

    /**
     * @param notificationInfo contains contents and metadata about a media notification
     *         that should be shown.
     */
    public abstract void showNotification(MediaNotificationInfo notificationInfo);

    /** Returns the ID to be used for Presentation API notifications. */
    public abstract int getPresentationNotificationId();

    /** Returns the ID to be used for Remote Playback API notifications. */
    public abstract int getRemotingNotificationId();

    /**
     * @param initiator the web contents that initiated the request.
     * @return a {@link FragmentManager} suitable for displaying a media router {@link
     *         DialogFragment} in.
     */
    public abstract FragmentManager getSupportFragmentManager(WebContents initiator);

    /** Runs deferredTask on the main thread when the main thread is idle. */
    public abstract void addDeferredTask(Runnable deferredTask);
}
