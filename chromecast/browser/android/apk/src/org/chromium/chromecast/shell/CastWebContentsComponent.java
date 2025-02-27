// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.net.Uri;
import android.os.IBinder;
import android.os.PatternMatcher;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chromecast.base.Controller;
import org.chromium.content_public.browser.WebContents;

/**
 * A layer of indirection between CastContentWindowAndroid and CastWebContents(Activity|Service).
 *
 * <p>If running in "headless" mode, it will use CastWebContentsService; otherwise, it will use
 * CastWebContentsActivity.
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

    @VisibleForTesting
    interface Delegate {
        void start(StartParams params);

        void stop();
    }

    @VisibleForTesting
    class ActivityDelegate implements Delegate {
        private static final String TAG = "CastWebContent_AD";
        private boolean mStarted;

        @Override
        public void start(StartParams params) {
            if (mStarted) return; // No-op if already started.
            Log.d(TAG, "start: SHOW_WEB_CONTENT in activity");
            startCastActivity(
                    params.webContents,
                    mEnableTouchInput,
                    params.shouldRequestAudioFocus,
                    mTurnOnScreen);
            mStarted = true;
        }

        @Override
        public void stop() {
            sendStopWebContentEvent();
            mStarted = false;
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
        Log.d(TAG, "stop: send STOP_WEB_CONTENT intent: " + intent);
        sendIntentSync(intent);
        sResumeIntent.reset();
    }

    private class ServiceDelegate implements Delegate {
        private static final String TAG = "CastWebContent_SD";

        private ServiceConnection mConnection =
                new ServiceConnection() {
                    @Override
                    public void onServiceConnected(ComponentName name, IBinder service) {}

                    @Override
                    public void onServiceDisconnected(ComponentName name) {
                        Log.d(TAG, "onServiceDisconnected");

                        if (mComponentClosedHandler != null) {
                            mComponentClosedHandler.onComponentClosed();
                        }
                    }
                };

        @Override
        public void start(StartParams params) {
            Log.d(TAG, "start");
            Intent intent =
                    CastWebContentsIntentUtils.requestStartCastService(
                            params.webContents, mSessionId);
            ContextUtils.getApplicationContext()
                    .bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
        }

        @Override
        public void stop() {
            Log.d(TAG, "stop");
            ContextUtils.getApplicationContext().unbindService(mConnection);
        }
    }

    public static final Controller<Intent> sResumeIntent = new Controller<Intent>();

    private static final String TAG = "CastWebComponent";

    private final OnComponentClosedHandler mComponentClosedHandler;
    private final String mSessionId;
    private final SurfaceEventHandler mSurfaceEventHandler;
    private final Controller<WebContents> mHasWebContentsState = new Controller<>();
    private Delegate mDelegate;
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
                "New CastWebContentsComponent: sid=%s, touchInput=%b, turnOnScreen=%b,"
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
            Log.d(TAG, "onReceive ACTION_ACTIVITY_STOPPED instance=" + mSessionId);
            if (mComponentClosedHandler != null) mComponentClosedHandler.onComponentClosed();
        } else if (CastWebContentsIntentUtils.isIntentOfVisibilityChange(intent)) {
            int visibilityType = CastWebContentsIntentUtils.getVisibilityType(intent);
            Log.d(
                    TAG,
                    "onReceive ACTION_ON_VISIBILITY_CHANGE instance="
                            + mSessionId
                            + "; visibilityType="
                            + visibilityType);
            if (mSurfaceEventHandler != null) {
                mSurfaceEventHandler.onVisibilityChange(visibilityType);
            }
        } else if (CastWebContentsIntentUtils.isIntentOfRequestMediaPlayingStatus(intent)) {
            Log.d(TAG, "onReceive ACTION_REQUEST_MEDIA_PLAYING_STATUS instance=" + mSessionId);
            // Just broadcast current value.
            setMediaPlaying(mMediaPlaying);
        }
    }

    @VisibleForTesting
    boolean isStarted() {
        return mStarted;
    }

    public void start(StartParams params, boolean isHeadless) {
        if (isHeadless) {
            Log.d(TAG, "Creating service delegate...");
            start(params, new ServiceDelegate());
        } else {
            Log.d(TAG, "Creating activity delegate...");
            start(params, new ActivityDelegate());
        }
    }

    @VisibleForTesting
    void start(StartParams params, Delegate delegate) {
        mDelegate = delegate;
        Log.d(
                TAG,
                "Starting WebContents with delegate: "
                        + mDelegate.getClass().getSimpleName()
                        + "; Instance ID: "
                        + mSessionId
                        + "; App ID: "
                        + params.appId
                        + "; shouldRequestAudioFocus: "
                        + params.shouldRequestAudioFocus);
        mHasWebContentsState.set(params.webContents);
        mDelegate.start(params);
        mStarted = true;
    }

    public void stop() {
        if (!mStarted) return;
        Log.d(
                TAG,
                "stop with delegate: "
                        + mDelegate.getClass().getSimpleName()
                        + "; Instance ID: "
                        + mSessionId);
        mHasWebContentsState.reset();
        Log.d(TAG, "Call delegate to stop");
        mDelegate.stop();
        mStarted = false;
    }

    public void enableTouchInput(boolean enabled) {
        Log.d(TAG, "enableTouchInput enabled:" + enabled);
        mEnableTouchInput = enabled;
        sendIntentSync(CastWebContentsIntentUtils.enableTouchInput(mSessionId, enabled));
    }

    public void setAllowPictureInPicture(boolean allowPictureInPicture) {
        Log.d(TAG, "setAllowPictureInPicture: " + allowPictureInPicture);
        sendIntentSync(
                CastWebContentsIntentUtils.allowPictureInPicture(
                        mSessionId, allowPictureInPicture));
    }

    public void setMediaPlaying(boolean mediaPlaying) {
        Log.d(TAG, "setMediaPlaying: " + mediaPlaying);
        mMediaPlaying = mediaPlaying;
        sendIntentSync(CastWebContentsIntentUtils.mediaPlaying(mSessionId, mMediaPlaying));
    }

    public static void onComponentClosed(String sessionId) {
        Log.d(TAG, "onComponentClosed");
        sendIntentSync(CastWebContentsIntentUtils.onActivityStopped(sessionId));
    }

    public static void onVisibilityChange(String sessionId, int visibilityType) {
        Log.d(TAG, "onVisibilityChange");
        sendIntentSync(CastWebContentsIntentUtils.onVisibilityChange(sessionId, visibilityType));
    }

    private static void sendIntentSync(Intent in) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(in);
    }
}
