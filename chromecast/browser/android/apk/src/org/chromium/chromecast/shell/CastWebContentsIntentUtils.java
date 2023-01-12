// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;

/**
 * Utils for creating and handling intents used by {@link CastWebContentsComponent} and
 * classes communicate with it.
 */
public class CastWebContentsIntentUtils {
    private static final String TAG = "CastWebUtil";

    static final String ACTION_DATA_SCHEME = "cast";
    static final String ACTION_DATA_AUTHORITY = "webcontents";

    private static final boolean DEBUG = true;

    /**
     * Action type of intent from Android to cast app to notify the stop event of
     * CastWebContentsActivity.
     */
    static final String ACTION_ACTIVITY_STOPPED =
            "com.google.android.apps.castshell.intent.action.ACTIVITY_STOPPED";

    /**
     * Action type of intent from Android to cast app to notify the visibility change
     * of cast app in an Android app.
     */
    public static final String ACTION_ON_VISIBILITY_CHANGE =
            "com.google.android.apps.castshell.intent.action.ON_VISIBILITY_CHANGE";

    /**
     * Action type of intent from CastWebContentsComponent to notify CastWebContentsActivity that
     * touch should be enabled.
     */
    public static final String ACTION_ENABLE_TOUCH_INPUT =
            "com.google.android.apps.castshell.intent.action.ENABLE_TOUCH_INPUT";

    /**
     * Action to notify CastWebContentsActivity whether PiP is allowed.
     */
    public static final String ACTION_ALLOW_PICTURE_IN_PICTURE =
            "com.google.android.apps.castshell.intent.action.ALLOW_PICTURE_IN_PICTURE";

    /**
     * Action to notify CastWebContentsActivity whether or not media is playing.
     */
    public static final String ACTION_MEDIA_PLAYING =
            "com.google.android.apps.castshell.intent.action.MEDIA_PLAYING";

    /**
     * Action to request that ACTION_MEDIA_PLAYING status is broadcasted.
     */
    public static final String ACTION_REQUEST_MEDIA_PLAYING_STATUS =
            "com.google.android.apps.castshell.intent.action.REQUEST_MEDIA_PLAYING";

    /** Key of extra value in an intent, the value is a URI of cast://webcontents/<instanceId> */
    static final String INTENT_EXTRA_URI = "content_uri";

    /** Key of extra value of the intent to start a web content, value is app ID of cast app */
    static final String INTENT_EXTRA_APP_ID = "content_app_id";

    /** Key of extra value of the intent to start a web content, value is session ID of cast app */
    static final String INTENT_EXTRA_SESSION_ID = "content_session_id";

    /** Key of extra value of the intent to start a web content, value is true if cast app supports
     *  touch input.
     */
    static final String INTENT_EXTRA_TOUCH_INPUT_ENABLED =
            "com.google.android.apps.castshell.intent.extra.ENABLE_TOUCH";

    /**
     *  Key of extra indicating whether PiP is allowed.
     */
    static final String INTENT_EXTRA_ALLOW_PICTURE_IN_PICTURE =
            "com.google.android.apps.castshell.intent.extra.ALLOW_PICTURE_IN_PICTURE";

    /**
     * Key of extra indicating whether media is playing.
     */
    static final String INTENT_EXTRA_MEDIA_PLAYING =
            "com.google.android.apps.castshell.intent.extra.MEDIA_PLAYING";

    /** Key of extra value of the intent to start a web content, value is true is if cast app is
     *  a remote control app.
     */
    static final String INTENT_EXTRA_SHOULD_REQUEST_AUDIO_FOCUS =
            "com.google.android.apps.castshell.intent.extra.SHOULD_REQUEST_AUDIO_FOCUS";

    /** Key for extra value for intent to start web contents. true if the app should turn on the
     * display. */
    static final String INTENT_EXTRA_TURN_ON_SCREEN =
            "com.google.android.apps.castshell.intent.extra.TURN_ON_SCREEN";

    /**
     * Key for extra value fot intent to start web contents. true if the app should keep the
     * screen on.
     */
    static final String INTENT_EXTRA_KEEP_SCREEN_ON =
            "com.google.android.apps.castshell.intent.extra.KEEP_SCREEN_ON";

    /**
     * Key of extra value of the intent ACTION_REQUEST_VISIBILITY, value is visibility priority
     * (int).
     */
    static final String INTENT_EXTRA_VISIBILITY_PRIORITY =
            "com.google.android.apps.castshell.intent.extra.content_visibility_priority";

    /** Key of extra value of the intent to start a web content, value is true is touch is enabled.
     */
    private static final String INTENT_EXTRA_KEY_CODE =
            "com.google.android.apps.castshell.intent.extra.KEY_CODE";

    /**
     * Key of extra value of the intent ACTION_ON_VISIBILITY_CHANGE, value is visibility type
     * (int).
     */
    private static final String INTENT_EXTRA_VISIBILITY_TYPE =
            "com.google.android.apps.castshell.intent.extra.VISIBILITY_TYPE";

    @VisibilityType
    static final int VISIBITY_TYPE_UNKNOWN = VisibilityType.UNKNOWN;
    @VisibilityType
    static final int VISIBITY_TYPE_FULL_SCREEN = VisibilityType.FULL_SCREEN;
    @VisibilityType
    static final int VISIBITY_TYPE_PARTIAL_OUT = VisibilityType.PARTIAL_OUT;
    @VisibilityType
    static final int VISIBITY_TYPE_HIDDEN = VisibilityType.HIDDEN;
    @VisibilityType
    static final int VISIBITY_TYPE_TRANSIENTLY_HIDDEN = VisibilityType.TRANSIENTLY_HIDDEN;

    // CastWebContentsSurfaceHelper -> CastWebContentsComponent.Receiver
    // -> CastContentWindowAndroid
    public static Intent onActivityStopped(String instanceId) {
        Intent intent = new Intent(ACTION_ACTIVITY_STOPPED, getInstanceUri(instanceId));
        return intent;
    }

    // CastWebContentsActivity -> CastWebContentsComponent.Receiver -> CastContentWindowAndroid
    public static Intent onVisibilityChange(String instanceId, @VisibilityType int visibilityType) {
        return onVisibilityChange(getInstanceUri(instanceId), visibilityType);
    }

    private static Intent onVisibilityChange(Uri uri, @VisibilityType int visibilityType) {
        if (DEBUG) Log.d(TAG, "onVisibilityChange with uri:" + uri + " type:" + visibilityType);

        Intent intent = new Intent(ACTION_ON_VISIBILITY_CHANGE, uri);
        intent.putExtra(INTENT_EXTRA_VISIBILITY_TYPE, visibilityType);
        return intent;
    }

    public static Intent requestMediaPlayingStatus(String instanceId) {
        Uri uri = getInstanceUri(instanceId);
        if (DEBUG) Log.d(TAG, "requestMediaPlayingStatus with uri: " + uri);
        return new Intent(ACTION_REQUEST_MEDIA_PLAYING_STATUS, uri);
    }

    // Used by intent of ACTION_ON_VISIBILITY_CHANGE
    @VisibilityType
    public static int getVisibilityType(Intent in) {
        return in.getIntExtra(INTENT_EXTRA_VISIBILITY_TYPE, 0);
    }

    public static boolean isIntentOfActivityStopped(Intent in) {
        return in.getAction().equals(ACTION_ACTIVITY_STOPPED);
    }

    public static boolean isIntentOfVisibilityChange(Intent in) {
        return in.getAction().equals(ACTION_ON_VISIBILITY_CHANGE);
    }

    public static boolean isIntentOfRequestMediaPlayingStatus(Intent in) {
        return in.getAction().equals(ACTION_REQUEST_MEDIA_PLAYING_STATUS);
    }

    // CastWebContentsComponent.Receiver -> CastWebContentsActivity
    public static Intent requestStartCastActivity(Context context, WebContents webContents,
            boolean enableTouch, boolean shouldRequestAudioFocus, boolean turnOnScreen,
            boolean keepScreenOn, String instanceId) {
        WebContentsRegistry.addWebContents(instanceId, webContents);
        Intent intent =
                new Intent(Intent.ACTION_VIEW, null, context, CastWebContentsActivity.class);
        intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(instanceId).toString());
        intent.putExtra(INTENT_EXTRA_SESSION_ID, instanceId);
        intent.putExtra(INTENT_EXTRA_TOUCH_INPUT_ENABLED, enableTouch);
        intent.putExtra(INTENT_EXTRA_TURN_ON_SCREEN, turnOnScreen);
        intent.putExtra(INTENT_EXTRA_KEEP_SCREEN_ON, keepScreenOn);
        intent.putExtra(INTENT_EXTRA_SHOULD_REQUEST_AUDIO_FOCUS, shouldRequestAudioFocus);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_TASK_ON_HOME);
        return intent;
    }

    // CastWebContentsComponent.Receiver -> CastWebContentsService
    public static Intent requestStartCastService(
            Context context, WebContents webContents, String instanceId) {
        WebContentsRegistry.addWebContents(instanceId, webContents);
        Intent intent = new Intent(Intent.ACTION_VIEW, getInstanceUri(instanceId), context,
                CastWebContentsService.class);
        intent.putExtra(INTENT_EXTRA_SESSION_ID, instanceId);
        return intent;
    }

    // CastWebContentsComponent.Delegate -> CastWebContentsSurfaceHelper
    public static Intent requestStopWebContents(String instanceId) {
        WebContentsRegistry.removeWebContents(instanceId);
        Intent intent = new Intent(CastIntents.ACTION_STOP_WEB_CONTENT);
        intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(instanceId).toString());
        return intent;
    }

    // Used by ACTION_VIEW
    public static String getSessionId(Bundle bundle) {
        return bundle.getString(INTENT_EXTRA_SESSION_ID);
    }

    // Used by ACTION_VIEW
    public static String getSessionId(Intent in) {
        return getSessionId(in.getExtras());
    }

    // Used by ACTION_VIEW
    public static WebContents getWebContents(Bundle bundle) {
        String sessionId = bundle.getString(INTENT_EXTRA_SESSION_ID);
        return WebContentsRegistry.getWebContents(sessionId);
    }

    // Used by ACTION_VIEW
    public static WebContents getWebContents(Intent in) {
        return getWebContents(in.getExtras());
    }

    // Used by ACTION_VIEW
    public static String getUriString(Bundle bundle) {
        return bundle.getString(INTENT_EXTRA_URI);
    }

    // Used by ACTION_VIEW
    public static String getUriString(Intent in) {
        return getUriString(in.getExtras());
    }

    // Used by ACTION_VIEW
    public static boolean isTouchable(Bundle bundle) {
        return bundle.getBoolean(INTENT_EXTRA_TOUCH_INPUT_ENABLED);
    }

    // Used by ACTION_VIEW
    public static boolean isTouchable(Intent in) {
        return isTouchable(in.getExtras());
    }

    // Used by ACTION_ALLOW_PICTURE_IN_PICTURE
    public static boolean isPictureInPictureAllowed(Intent in) {
        return in.getExtras().getBoolean(INTENT_EXTRA_ALLOW_PICTURE_IN_PICTURE);
    }

    // Used by ACTION_MEDIA_PLAYING
    public static boolean isMediaPlaying(Intent in) {
        return in.getExtras().getBoolean(INTENT_EXTRA_MEDIA_PLAYING);
    }

    // Used by ACTION_VIEW
    public static boolean shouldRequestAudioFocus(Bundle bundle) {
        return bundle.getBoolean(INTENT_EXTRA_SHOULD_REQUEST_AUDIO_FOCUS);
    }

    // Used by ACTION_VIEW
    public static boolean shouldRequestAudioFocus(Intent in) {
        return shouldRequestAudioFocus(in.getExtras());
    }

    // Used by ACTION_VIEW
    public static boolean shouldTurnOnScreen(Intent intent) {
        return intent.getBooleanExtra(INTENT_EXTRA_TURN_ON_SCREEN, true);
    }

    // Used by ACTION_VIEW and ACTION_SHOW_WEB_CONTENT
    public static boolean shouldKeepScreenOn(Intent intent) {
        return intent.getBooleanExtra(INTENT_EXTRA_KEEP_SCREEN_ON, false);
    }

    // CastWebContentsComponent -> CastWebContentsSurfaceHelper
    public static Intent enableTouchInput(String instanceId, boolean enabled) {
        Intent intent = new Intent(ACTION_ENABLE_TOUCH_INPUT);
        intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(instanceId).toString());
        intent.putExtra(INTENT_EXTRA_TOUCH_INPUT_ENABLED, enabled);
        return intent;
    }

    // CastWebContentsComponent -> CastWebContentsActivity
    public static Intent allowPictureInPicture(String instanceId, boolean allowPictureInPicture) {
        Intent intent = new Intent(ACTION_ALLOW_PICTURE_IN_PICTURE);
        intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(instanceId).toString());
        intent.putExtra(INTENT_EXTRA_ALLOW_PICTURE_IN_PICTURE, allowPictureInPicture);
        return intent;
    }

    public static Intent mediaPlaying(String instanceId, boolean mediaPlaying) {
        Intent intent = new Intent(ACTION_MEDIA_PLAYING);
        intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(instanceId).toString());
        intent.putExtra(INTENT_EXTRA_MEDIA_PLAYING, mediaPlaying);
        return intent;
    }

    // CastWebContentsSurfaceHelper -> CastWebContentsActivity
    public static Intent onWebContentStopped(Uri uri) {
        Intent intent = new Intent(CastIntents.ACTION_ON_WEB_CONTENT_STOPPED);
        intent.putExtra(INTENT_EXTRA_URI, uri.toString());
        return intent;
    }

    public static Uri getInstanceUri(String instanceId) {
        Uri instanceUri = new Uri.Builder()
                                  .scheme(ACTION_DATA_SCHEME)
                                  .authority(ACTION_DATA_AUTHORITY)
                                  .path(instanceId)
                                  .build();
        return instanceUri;
    }

    static LocalBroadcastManager getLocalBroadcastManager() {
        return LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext());
    }
}
