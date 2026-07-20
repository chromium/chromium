// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.chromium.chromecast.base.Observable.any;
import static org.chromium.chromecast.base.Observable.not;

import android.app.ActivityManager;
import android.app.PictureInPictureParams;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import androidx.activity.ComponentActivity;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.chromecast.base.Both;
import org.chromium.chromecast.base.CastSwitches;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Dict;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Observer;
import org.chromium.chromecast.base.Unit;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.Objects;

/**
 * Activity for displaying a WebContents in CastShell.
 *
 * <p>Typically, this class is controlled by CastContentWindowAndroid through
 * CastWebContentsSurfaceHelper. CastContentWindowAndroid which will start a new instance of this
 * activity. If the CastContentWindowAndroid is destroyed, CastWebContentsActivity should finish().
 * Similarily, if this activity is destroyed, CastContentWindowAndroid should be notified by intent.
 */
public class CastWebContentsActivity extends ComponentActivity {
    private static final String TAG = "CastWebActivity";

    @VisibleForTesting
    static class MediaPlaying {
        public final boolean hasAudio;
        public final boolean hasVideo;

        public MediaPlaying(boolean hasAudio, boolean hasVideo) {
            this.hasAudio = hasAudio;
            this.hasVideo = hasVideo;
        }

        @Override
        public boolean equals(Object other) {
            if (other instanceof MediaPlaying) {
                MediaPlaying that = (MediaPlaying) other;
                return this.hasAudio == that.hasAudio && this.hasVideo == that.hasVideo;
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(hasAudio, hasVideo);
        }

        @Override
        public String toString() {
            return "MediaPlaying{hasAudio=" + hasAudio + ", hasVideo=" + hasVideo + "}";
        }

        public static Observable<MediaPlaying> observeFromWebContents(WebContents webContents) {
            return observer -> {
                Dict<Integer, MediaPlaying> dict = new Dict<>();
                var webContentsObserver =
                        new WebContentsObserver(webContents) {
                            @Override
                            public void mediaStartedPlaying(
                                    int id, boolean hasAudio, boolean hasVideo) {
                                dict.put(id, new MediaPlaying(hasAudio, hasVideo));
                            }

                            @Override
                            public void mediaStoppedPlaying(int id) {
                                dict.remove(id);
                            }
                        };
                return dict.values()
                        .subscribe(observer)
                        .and(() -> webContentsObserver.observe(null));
            };
        }
    }

    // Tracks whether this Activity is between onCreate() and onDestroy().
    private final Controller<Unit> mCreatedState = new Controller<>();
    // Tracks whether this Activity is between onStart() and onStop().
    private final Controller<Unit> mStartedState = new Controller<>();
    // Tracks the most recent Intent for the Activity.
    private final Controller<Intent> mGotIntentState = new Controller<>();
    // Tracks the most recent session id for the Activity. Derived from
    // mGotIntentState.
    private final Controller<String> mSessionIdState = new Controller<>();
    // Set this to cause the Activity to finish.
    private final Controller<String> mIsFinishingState = new Controller<>();
    // Set in unittests to skip some behavior.
    private final Controller<Unit> mIsTestingState = new Controller<>();
    // Set at creation. Handles destroying SurfaceHelper.
    private final Controller<CastWebContentsSurfaceHelper> mSurfaceHelperState = new Controller<>();
    // Set when the activity has the surface available.
    @VisibleForTesting final Controller<Unit> mSurfaceAvailable = new Controller<>();

    @Nullable private CastWebContentsSurfaceHelper mSurfaceHelper;

    // SessionId provided in the original Intent used to start the Activity.
    private String mRootSessionId;

    private boolean mAudioIsPlaying;
    private boolean mVideoIsPlaying;
    private boolean mIsInPictureInPictureMode;

    {
        Observable<Intent> gotIntentAfterFinishingState =
                mIsFinishingState.ignoreAndThen(mGotIntentState);
        Observable<?> createdAndNotTestingState = mCreatedState.and(not(mIsTestingState));
        createdAndNotTestingState.subscribe(
                Observer.onOpen(
                        x -> {
                            // Abort if the browser process has not been initialized. This can
                            // happen in exotic race conditions where CastBrowserService kills the
                            // process before the teardown timer in CastWebContentsSurfaceHelper
                            // fires.
                            if (!CastBrowserHelper.isBrowserInitialized()) {
                                finishAndRemoveTask();
                                return;
                            }

                            setContentView(R.layout.cast_web_contents_activity);

                            mSurfaceHelperState.set(
                                    new CastWebContentsSurfaceHelper(
                                            CastWebContentsScopes.onLayoutActivity(
                                                    this,
                                                    (FrameLayout)
                                                            findViewById(
                                                                    R.id.web_contents_container),
                                                    CastSwitches.getSwitchValueColor(
                                                            CastSwitches.CAST_APP_BACKGROUND_COLOR,
                                                            Color.BLACK)),
                                            (Uri uri) ->
                                                    mIsFinishingState.set(
                                                            "Delayed teardown for URI: " + uri),
                                            mSurfaceAvailable));
                        }));

        mSurfaceHelperState.subscribe(
                (CastWebContentsSurfaceHelper surfaceHelper) -> {
                    mSurfaceHelper = surfaceHelper;
                    return () -> {
                        surfaceHelper.onDestroy();
                        mSurfaceHelper = null;
                    };
                });

        mCreatedState.subscribe(Observer.onClose(x -> mSurfaceHelperState.reset()));

        final Controller<Unit> isDocked = new Controller<>();
        mCreatedState.subscribe(
                x -> {
                    IntentFilter filter = new IntentFilter(Intent.ACTION_DOCK_EVENT);
                    return new BroadcastReceiverScope(
                            filter,
                            (Intent intent) -> {
                                if (isDocked(intent)) {
                                    isDocked.set(Unit.unit());
                                } else {
                                    isDocked.reset();
                                }
                            });
                });

        mCreatedState.subscribe(
                x -> {
                    IntentFilter filter = new IntentFilter(Intent.ACTION_USER_PRESENT);
                    return new BroadcastReceiverScope(
                            filter,
                            (Intent intent) -> {
                                Log.d(
                                        TAG,
                                        "ACTION_USER_PRESENT received. canUsePictureInPicture: "
                                                + canUsePictureInPicture()
                                                + " mVideoIsPlaying: "
                                                + mVideoIsPlaying);
                                if (canUsePictureInPicture() && mVideoIsPlaying) {
                                    enterPictureInPictureMode(
                                            new PictureInPictureParams.Builder().build());
                                }
                            });
                });

        Observable<Unit> shouldKeepScreenOn =
                mGotIntentState
                        .filter(
                                intent ->
                                        CastWebContentsIntentUtils.shouldKeepScreenOn(intent)
                                                || isInLockTaskMode(this))
                        .opaque();

        isDocked.subscribe(
                (x) -> {
                    getWindow()
                            .addFlags(WindowManager.LayoutParams.FLAG_ALLOW_LOCK_WHILE_SCREEN_ON);
                    return () ->
                            getWindow()
                                    .clearFlags(
                                            WindowManager.LayoutParams
                                                    .FLAG_ALLOW_LOCK_WHILE_SCREEN_ON);
                });

        mGotIntentState
                .map(Intent::getExtras)
                .map(CastWebContentsIntentUtils::getSessionId)
                .subscribe(Observer.onOpen(mSessionIdState::set));

        mStartedState
                .and(mSessionIdState)
                .subscribe(
                        both -> {
                            sendVisibilityChanged(
                                    both.second,
                                    CastWebContentsIntentUtils.VISIBITY_TYPE_FULL_SCREEN);
                            return () -> {
                                sendVisibilityChanged(
                                        both.second,
                                        CastWebContentsIntentUtils.VISIBITY_TYPE_HIDDEN);
                            };
                        });
        mStartedState.subscribe(Observer.onOpen(mSurfaceAvailable::set));

        // Set a flag to exit sleep mode when this activity starts.
        mCreatedState
                .ignoreAnd(mGotIntentState)
                // Turn the screen on only if the launching Intent asks to.
                .filter(CastWebContentsIntentUtils::shouldTurnOnScreen)
                .subscribe(Observer.onOpen(x -> turnScreenOn()));

        // Handle each new Intent.
        Controller<CastWebContentsSurfaceHelper.StartParams> startParamsState = new Controller<>();
        mGotIntentState
                .andIgnore(not(mIsFinishingState))
                .map(Intent::getExtras)
                .map(CastWebContentsSurfaceHelper.StartParams::fromBundle)
                // Use the duplicate-filtering functionality of Controller to drop duplicate params.
                .subscribe(Observer.onOpen(startParamsState::set));
        mSurfaceHelperState
                .and(startParamsState)
                .subscribe(
                        Observer.onOpen(
                                Both.adapt(CastWebContentsSurfaceHelper::onNewStartParams)));

        final Observable<WebContents> webContentsState =
                mCreatedState.ignoreAnd(startParamsState).map(params -> params.webContents);
        final Observable<MediaPlaying> mediaPlaying =
                webContentsState.flatMap(MediaPlaying::observeFromWebContents).share();
        final var audioPlaying = any(mediaPlaying.filter(x -> x.hasAudio));
        final var videoPlaying = any(mediaPlaying.filter(x -> x.hasVideo));
        final var anyMediaPlaying = any(audioPlaying.or(videoPlaying));

        videoPlaying.subscribe(
                x -> {
                    Log.i(TAG, "video playing");
                    mVideoIsPlaying = true;
                    return () -> {
                        Log.i(TAG, "video stopped");
                        mVideoIsPlaying = false;
                    };
                });
        audioPlaying.subscribe(
                x -> {
                    Log.i(TAG, "audio playing");
                    mAudioIsPlaying = true;
                    return () -> {
                        Log.i(TAG, "audio stopped");
                        mAudioIsPlaying = false;
                    };
                });

        any(shouldKeepScreenOn
                        .or(anyMediaPlaying.and(isDocked).opaque())
                        .or(audioPlaying.filter(x -> !canPlayBackgroundAudio()).opaque()))
                .subscribe(
                        (x) -> {
                            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                            return () ->
                                    getWindow()
                                            .clearFlags(
                                                    WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                        });

        mGotIntentState
                .andIgnore(not(mIsFinishingState))
                .map(CastWebContentsIntentUtils::getSessionId)
                .subscribe(
                        sessionId -> {
                            TaskRemovedMonitorService.start(mRootSessionId, sessionId);
                            return () -> TaskRemovedMonitorService.stop();
                        });

        mIsFinishingState.subscribe(
                Observer.onOpen(
                        (String reason) -> {
                            Log.d(TAG, "Finishing activity: %s", reason);
                            mSurfaceHelperState.reset();
                            finishAndRemoveTask();
                        }));

        // If a new Intent arrives after finishing, start a new Activity instead of recycling this.
        gotIntentAfterFinishingState.subscribe(
                Observer.onOpen(
                        (Intent intent) -> {
                            Log.d(
                                    TAG,
                                    "Got intent while finishing current activity, so start new"
                                            + " activity.");
                            int flags = intent.getFlags();
                            flags = flags & ~Intent.FLAG_ACTIVITY_SINGLE_TOP;
                            intent.setFlags(flags);
                            startActivity(intent);
                        }));

        var backPressAdapter = BackPressCallbackAdapter.create(this, getOnBackPressedDispatcher());
        webContentsState
                .andIgnore(backPressAdapter.observeBackPressedEvents())
                .flatMap(this::handleBackPressedEvent)
                // handleBackPressedEvent() emits a signal only if the handler didn't consume the
                // event; if this happens, we need to re-dispatch the back press event to the
                // fallback callback.
                .subscribe(
                        Observer.onOpen(x -> backPressAdapter.fallbackToDefaultBackPressHandler()));
    }

    @RequiresApi(Build.VERSION_CODES.S)
    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mCreatedState.set(Unit.unit());
        Intent intent = getIntent();
        if (intent != null) {
            mRootSessionId = CastWebContentsIntentUtils.getSessionId(intent);
            Log.d(TAG, "Activity created: rootSessionId=%s", mRootSessionId);
        }
        // This is a no-op if `intent` is null.
        mGotIntentState.set(intent);

        // Whenever our app is visible, volume controls should modify the music stream.
        // For more information read:
        // http://developer.android.com/training/managing-audio/volume-playback.html
        setVolumeControlStream(AudioManager.STREAM_MUSIC);

        // switch to fullscreen (immersive) mode
        getWindow()
                .getDecorView()
                .setSystemUiVisibility(
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        notifyIfActivityStartedByCastCore(getIntent());
    }

    private static final String EXTRA_IS_DELAYED_EXPANSION =
            "com.google.android.apps.castshell.extra.IS_DELAYED_EXPANSION";

    @Override
    protected void onNewIntent(Intent intent) {
        Log.d(TAG, "onNewIntent");
        super.onNewIntent(intent);
        setIntent(intent);
        mGotIntentState.set(intent);
        notifyIfActivityStartedByCastCore(intent);

        boolean isDelayedExpansion = intent.getBooleanExtra(EXTRA_IS_DELAYED_EXPANSION, false);
        if (mIsInPictureInPictureMode && !isDelayedExpansion) {
            Log.i(TAG, "onNewIntent received while in PiP mode. Posting delayed expansion.");
            Intent expandIntent = new Intent(intent);
            expandIntent.putExtra(EXTRA_IS_DELAYED_EXPANSION, true);
            expandIntent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
            new Handler(Looper.getMainLooper())
                    .postDelayed(
                            () -> {
                                Log.i(TAG, "Executing delayed expansion from PiP.");
                                startActivity(expandIntent);
                            },
                            300);
        }
    }

    @Override
    protected void onStart() {
        Log.d(TAG, "onStart");
        mStartedState.set(Unit.unit());
        super.onStart();
    }

    @Override
    protected void onStop() {
        Log.d(TAG, "onStop");
        mStartedState.reset();

        // If this device is in "lock task mode," then leaving the Activity will not return to the
        // Home screen and there will be no affordance for the user to return to this Activity.
        // When in this mode, leaving the Activity should tear down the Cast app.
        if (isInLockTaskMode(this)) {
            CastWebContentsComponent.onComponentClosed(
                    CastWebContentsIntentUtils.getSessionId(getIntent()));
            mIsFinishingState.set("User exit while in lock task mode");
        }
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, "onDestroy");

        mCreatedState.reset();
        super.onDestroy();
    }

    private static String loadBackPressedJavaScript(Context context)
            throws IOException, Resources.NotFoundException {
        try (InputStream inputStream = context.getResources().openRawResource(R.raw.back_pressed)) {
            return new String(FileUtils.readStream(inputStream), StandardCharsets.UTF_8);
        }
    }

    /**
     * Returns an Observable that activates if the back press event is NOT consumed by JavaScript.
     */
    private Observable<Unit> handleBackPressedEvent(WebContents webContents) {
        String backPressedJs;
        try {
            backPressedJs = loadBackPressedJavaScript(this);
        } catch (IOException | Resources.NotFoundException e) {
            Log.e(TAG, "Failed to find JS resource for handling back press key events", e);
            // Activate the observer synchronously to indicate that the event is not consumed.
            return Observable.just(Unit.unit());
        }

        Controller<Unit> defaultPreventedEvent = new Controller<>();
        webContents.evaluateJavaScript(
                backPressedJs,
                defaultPrevented -> {
                    if (!"true".equals(defaultPrevented)) {
                        defaultPreventedEvent.set(Unit.unit());
                    }
                });
        return defaultPreventedEvent;
    }

    private static boolean isInLockTaskMode(Context context) {
        ActivityManager activityManager = context.getSystemService(ActivityManager.class);
        return activityManager.getLockTaskModeState() != ActivityManager.LOCK_TASK_MODE_NONE;
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public void onUserLeaveHint() {
        Log.d(TAG, "onUserLeaveHint");
        super.onUserLeaveHint();
        if (canUsePictureInPicture() && mVideoIsPlaying) {
            Log.i(TAG, "entering picture-in-picture mode");
            enterPictureInPictureMode(new PictureInPictureParams.Builder().build());
        } else if (canPlayBackgroundAudio() && mAudioIsPlaying) {
            Log.i(TAG, "entering background audio mode");
        } else {
            mSurfaceAvailable.reset();
            CastWebContentsComponent.onComponentClosed(
                    CastWebContentsIntentUtils.getSessionId(getIntent()));
            mIsFinishingState.set("User exit while backgroundable media is not playing");
        }
    }

    @Override
    public void onPictureInPictureModeChanged(
            boolean isInPictureInPictureMode, Configuration newConfig) {
        if (newConfig != null) {
            super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
        }
        mIsInPictureInPictureMode = isInPictureInPictureMode;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (mIsInPictureInPictureMode
                || mSurfaceHelper == null
                || !mSurfaceHelper.isTouchInputEnabled()) {
            return false;
        }
        return super.dispatchTouchEvent(ev);
    }

    private void turnScreenOn() {
        Log.i(TAG, "Setting flag to turn screen on");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setTurnScreenOn(true);
            // Allow Activities that turn on the screen to show in the lock screen.
            setShowWhenLocked(true);
        } else {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
        }
    }

    private boolean canUsePictureInPicture() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && getPackageManager().hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)
                && !DeviceInfo.isTV();
    }

    private boolean canPlayBackgroundAudio() {
        return !DeviceInfo.isTV();
    }

    private static boolean isDocked(Intent intent) {
        if (intent == null || !Intent.ACTION_DOCK_EVENT.equals(intent.getAction())) {
            Log.w(TAG, "Invalid dock intent:" + intent);
            return false;
        }
        int dockState = intent.getIntExtra(Intent.EXTRA_DOCK_STATE, -1);
        return dockState != Intent.EXTRA_DOCK_STATE_UNDOCKED;
    }

    // Sends the specified visibility change event to the current app (as reported by getIntent()).
    private void sendVisibilityChanged(String sessionId, @VisibilityType int visibilityType) {
        Context ctx = getApplicationContext();
        Intent event = CastWebContentsIntentUtils.onVisibilityChange(sessionId, visibilityType);
        LocalBroadcastManager.getInstance(ctx).sendBroadcastSync(event);
    }

    private void notifyIfActivityStartedByCastCore(Intent in) {
        if (!CastWebContentsIntentUtils.isIntentFromCastCore(in)) {
            return;
        }
        Log.d(TAG, "Notifying activity started by cast_core.");
        Context ctx = getApplicationContext();
        Intent intent = CastWebContentsIntentUtils.onActivityStartedByCastCore();
        LocalBroadcastManager.getInstance(ctx).sendBroadcastSync(intent);
    }

    public void finishForTesting() {
        mIsFinishingState.set("Finish for testing");
    }

    public void testingModeForTesting() {
        mIsTestingState.set(Unit.unit());
    }

    public void setSurfaceHelperForTesting(CastWebContentsSurfaceHelper surfaceHelper) {
        mSurfaceHelperState.set(surfaceHelper);
    }
}
