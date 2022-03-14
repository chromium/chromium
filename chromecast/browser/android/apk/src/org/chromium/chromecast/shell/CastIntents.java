// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

/**
 * A namespace for constants to uniquely describe certain public Intents that can be used to control
 * the life cycle of CastWebContentsActivity.
 */
public class CastIntents {
    public static final String ACTION_SCREEN_OFF =
            "com.google.android.apps.castshell.intent.action.ACTION_SCREEN_OFF";

    /**
     * Action type of intent from CastWebContentsComponent to host activity of
     * CastWebContentsFragment to show web contents. (To start CastWebContentsActivity use
     * Context.startActivity())
     */
    public static final String ACTION_SHOW_WEB_CONTENT = "com.google.assistant.SHOW_WEB_CONTENT";

    /**
     * Action type of intent from CastWebContentsComponent to mInternalStopReceiver to detach
     * WebContents and then file an intent to CastWebContentsFragment's host activity to stop
     * fragment.
     */
    public static final String ACTION_STOP_WEB_CONTENT = "com.google.assistant.STOP_WEB_CONTENT";

    /**
     * Action type of intent from CastWebContentsComponent to notify host activity of WebContents,
     * either CastWebContentsActivity or external activity, the web content has been stopped.
     * <p>
     * ACTION_ON_WEB_CONTENT_STOP is filed after intent ACTION_STOP_WEB_CONTENT is handled.
     * Both CastWebContentsAcitivty and external activity should handle this intent to either stop
     * itself or remove the fragment, or anything else base on its own logic.
     */
    public static final String ACTION_ON_WEB_CONTENT_STOPPED =
            "com.google.assistant.ON_WEB_CONTENT_STOPPED";

    public static final String ACTION_START_CAST_BROWSER =
            "com.google.cast.action.START_CAST_BROWSER";
}
