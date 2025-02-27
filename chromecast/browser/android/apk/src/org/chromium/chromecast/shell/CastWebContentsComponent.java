// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.PatternMatcher;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chromecast.base.Controller;
import org.chromium.content_public.browser.WebContents;

/**
 * A layer of indirection between CastContentWindowAndroid and CastWebContents(Activity|Service).
 */
public class CastWebContentsComponent {
    /**
     * Callback interface for when the associated component is closed or the WebContents is
     * detached.
     */
    public interface OnComponentClosedHandler {
        void onComponentClosed();
    }

    /** Callback interface for when UI events occur. */
    public interface SurfaceEventHandler {
        void onVisibilityChange(int visibilityType);
    }

    /** Params to start WebContents in activity or service. */
    static class StartParams {
        public final WebContents webContents;
        public final String appId;
        public final boolean shouldRequestAudioFocus;

        public StartParams(WebContents webContents, String appId, boolean shouldRequestAudioFocus) {
            this.webContents = webContents;
            this.appId = appId;
            this.shouldRequestAudioFocus = shouldRequestAudioFocus;
        }

        @Override
        public boolean equals(Object other) {
            if (other == this) {
                return true;
            }

            if (!(other instanceof StartParams)) {
                return false;
            }

            StartParams params = (StartParams) other;
            return params.webContents == this.webContents && params.appId.equals(this.appId);
        }
    }

    private void startCastActivity(
            WebContents webContents,
            boolean enableTouch,
            boolean shouldRequestAudioFocus,
            boolean turnOnScreen) {
        Intent intent =
                CastWebContentsIntentUtils.requestStartCastActivity(
                        webContents,
                        enableTouch,
                        shouldRequestAudioFocus,
                        turnOnScreen,
                        mKeepScreenOn,
                        mSessionId);
        sResumeIntent.set(intent);
        ContextUtils.getApplicationContext().startActivity(intent);
    }

    private void sendStopWebContentEvent() {
        Intent intent = CastWebContentsIntentUtils.requestStopWebContents(mSessionId);
        Log.d(TAG, "Requesting activity to stop: sessionId=%s", mSessionId);
        sendIntentSync(intent);
        sResumeIntent.reset();
    }

    public static final Controller<Intent> sResumeIntent = new Controller<Intent>();

    private static final String TAG = "CastWebComponent";

    private final OnComponentClosedHandler mComponentClosedHandler;
    private final String mSessionId;
    private final SurfaceEventHandler mSurfaceEventHandler;
    private final Controller<WebContents> mHasWebContentsState = new Controller<>();
    private boolean mStarted;
    private boolean mEnableTouchInput;
    private boolean mMediaPlaying;
    private final boolean mTurnOnScreen;
    private final boolean mKeepScreenOn;

    public CastWebContentsComponent(
            String sessionId,
            OnComponentClosedHandler onComponentClosedHandler,
            SurfaceEventHandler surfaceEventHandler,
            boolean enableTouchInput,
            boolean turnOnScreen,
            boolean keepScreenOn) {
        Log.d(
                TAG,
                "CastWebContentsComponent created: sessionId=%s, touchInput=%b, turnOnScreen=%b,"
                        + " keepScreenOn=%b",
                sessionId,
                enableTouchInput,
                turnOnScreen,
                keepScreenOn);

        mComponentClosedHandler = onComponentClosedHandler;
        mEnableTouchInput = enableTouchInput;
        mSessionId = sessionId;
        mSurfaceEventHandler = surfaceEventHandler;
        mTurnOnScreen = turnOnScreen;
        mKeepScreenOn = keepScreenOn;

        mHasWebContentsState.subscribe(
                x -> {
                    final IntentFilter filter = new IntentFilter();
                    Uri instanceUri = CastWebContentsIntentUtils.getInstanceUri(sessionId);
                    filter.addDataScheme(instanceUri.getScheme());
                    filter.addDataAuthority(instanceUri.getAuthority(), null);
                    filter.addDataPath(instanceUri.getPath(), PatternMatcher.PATTERN_LITERAL);
                    filter.addAction(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED);
                    filter.addAction(CastWebContentsIntentUtils.ACTION_ON_VISIBILITY_CHANGE);
                    filter.addAction(
                            CastWebContentsIntentUtils.ACTION_REQUEST_MEDIA_PLAYING_STATUS);
                    return new LocalBroadcastReceiverScope(filter, this::onReceiveIntent);
                });
    }

    private void onReceiveIntent(Intent intent) {
        if (CastWebContentsIntentUtils.isIntentOfActivityStopped(intent)) {
            Log.d(TAG, "Activity stopped: sessionId=" + mSessionId);
            if (mComponentClosedHandler != null) {
                mComponentClosedHandler.onComponentClosed();
            }
        } else if (CastWebContentsIntentUtils.isIntentOfVisibilityChange(intent)) {
            int visibilityType = CastWebContentsIntentUtils.getVisibilityType(intent);
            Log.d(
                    TAG,
                    "Activity visibility changed: sessionId=%s, visibilityType=%d",
                    mSessionId,
                    visibilityType);
            if (mSurfaceEventHandler != null) {
                mSurfaceEventHandler.onVisibilityChange(visibilityType);
            }
        } else if (CastWebContentsIntentUtils.isIntentOfRequestMediaPlayingStatus(intent)) {
            Log.d(
                    TAG,
                    "Activity media play state requested: sessionId=%s, mediaPlaying=%b",
                    mSessionId,
                    mMediaPlaying);
            // Just broadcast current value.
            setMediaPlaying(mMediaPlaying);
        }
    }

    @VisibleForTesting
    boolean isStarted() {
        return mStarted;
    }

    public void start(StartParams params) {
        Log.d(
                TAG,
                "Starting Cast activity: sessionId=%s, appId=%s, audioFocus=%b",
                mSessionId,
                params.appId,
                params.shouldRequestAudioFocus);

        mHasWebContentsState.set(params.webContents);
        startCastActivity(
                params.webContents,
                mEnableTouchInput,
                params.shouldRequestAudioFocus,
                mTurnOnScreen);
        mStarted = true;
    }

    public void stop() {
        if (!mStarted) {
            return;
        }

        Log.d(TAG, "Stopping WebContents: sessionId" + mSessionId);
        mHasWebContentsState.reset();
        sendStopWebContentEvent();
        mStarted = false;
    }

    public void enableTouchInput(boolean enabled) {
        Log.d(TAG, "Touch input updated: enabled=" + enabled);
        mEnableTouchInput = enabled;
        sendIntentSync(CastWebContentsIntentUtils.enableTouchInput(mSessionId, enabled));
    }

    public void setAllowPictureInPicture(boolean allowPictureInPicture) {
        Log.d(TAG, "PiP updated: allowed=" + allowPictureInPicture);
        sendIntentSync(
                CastWebContentsIntentUtils.allowPictureInPicture(
                        mSessionId, allowPictureInPicture));
    }

    public void setMediaPlaying(boolean mediaPlaying) {
        Log.d(TAG, "Media playing updated: playing=" + mediaPlaying);
        mMediaPlaying = mediaPlaying;
        sendIntentSync(CastWebContentsIntentUtils.mediaPlaying(mSessionId, mMediaPlaying));
    }

    public static void onComponentClosed(String sessionId) {
        Log.d(TAG, "Component closed: sessionId=" + sessionId);
        sendIntentSync(CastWebContentsIntentUtils.onActivityStopped(sessionId));
    }

    public static void onVisibilityChange(String sessionId, int visibilityType) {
        Log.d(
                TAG,
                "Visibility changed: sessionId=%s, visibilityType=%d",
                sessionId,
                visibilityType);
        sendIntentSync(CastWebContentsIntentUtils.onVisibilityChange(sessionId, visibilityType));
    }

    private static void sendIntentSync(Intent in) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(in);
    }
}
