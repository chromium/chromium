// Copyright 2017 The Chromium Authors
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
     * Used by CastWebContentsComponent to tell CastWebContentsSurfaceHelper to tear down the web
     * contents.
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
