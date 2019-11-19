// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Activity;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.chromium.base.Log;
import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chromecast.base.Both;
import org.chromium.chromecast.base.CastSwitches;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Observers;
import org.chromium.chromecast.base.Unit;

/**
 * Activity for displaying a WebContents in CastShell.
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid through
 * CastWebContentsSurfaceHelper. CastContentWindowAndroid which will
 * start a new instance of this activity. If the CastContentWindowAndroid is
 * destroyed, CastWebContentsActivity should finish(). Similarily, if this
 * activity is destroyed, CastContentWindowAndroid should be notified by intent.
 */
public class CastWebContentsActivity extends Activity {
    private static final String TAG = "CastWebActivity";
    private static final boolean DEBUG = true;

    // Tracks whether this Activity is between onCreate() and onDestroy().
    private final Controller<Unit> mCreatedState = new Controller<>();
    // Tracks whether this Activity is between onResume() and onPause().
    private final Controller<Unit> mResumedState = new Controller<>();
    // Tracks whether this Activity is between onStart() and onStop().
    private final Controller<Unit> mStartedState = new Controller<>();
    // Tracks whether the user has left according to onUserLeaveHint().
    private final Controller<Unit> mUserLeftState = new Controller<>();
    // Tracks the most recent Intent for the Activity.
    private final Controller<Intent> mGotIntentState = new Controller<>();
    // Set this to cause the Activity to finish.
    private final Controller<String> mIsFinishingState = new Controller<>();
    // Set this to provide the Activity with a CastAudioManager.
    private final Controller<CastAudioManager> mAudioManagerState = new Controller<>();
    // Set in unittests to skip some behavior.
    private final Controller<Unit> mIsTestingState = new Controller<>();
    // Set at creation. Handles destroying SurfaceHelper.
    private final Controller<CastWebContentsSurfaceHelper> mSurfaceHelperState = new Controller<>();

    @Nullable
    private CastWebContentsSurfaceHelper mSurfaceHelper;

    {
        Observable<Intent> gotIntentAfterFinishingState =
                mIsFinishingState.andThen(mGotIntentState).map(Both::getSecond);
        Observable<?> createdAndNotTestingState =
                mCreatedState.and(Observable.not(mIsTestingState));
        createdAndNotTestingState.subscribe(x -> {
            // Register handler for web content stopped event while we have an Intent.
            IntentFilter filter = new IntentFilter();
            filter.addAction(CastIntents.ACTION_ON_WEB_CONTENT_STOPPED);
            return new LocalBroadcastReceiverScope(filter, (Intent intent) -> {
                mIsFinishingState.set("Stopped by intent: " + intent.getAction());
            });
        });
        createdAndNotTestingState.subscribe(Observers.onEnter(x -> {
            // Do this in onCreate() only if not testing.
            if (!CastBrowserHelper.initializeBrowser(getApplicationContext())) {
                Toast.makeText(this, R.string.browser_process_initialization_failed,
                             Toast.LENGTH_SHORT)
                        .show();
                mIsFinishingState.set("Failed to initialize browser");
            }

            setContentView(R.layout.cast_web_contents_activity);

            mSurfaceHelperState.set(new CastWebContentsSurfaceHelper(
                    CastWebContentsScopes.onLayoutActivity(this,
                            (FrameLayout) findViewById(R.id.web_contents_container),
                            CastSwitches.getSwitchValueColor(
                                    CastSwitches.CAST_APP_BACKGROUND_COLOR, Color.BLACK)),
                    (Uri uri) -> mIsFinishingState.set("Delayed teardown for URI: " + uri)));
        }));

        mSurfaceHelperState.subscribe((CastWebContentsSurfaceHelper surfaceHelper) -> {
            mSurfaceHelper = surfaceHelper;
            return () -> {
                surfaceHelper.onDestroy();
                mSurfaceHelper = null;
            };
        });

        mCreatedState.subscribe(Observers.onExit(x -> mSurfaceHelperState.reset()));

        mCreatedState.map(x -> getWindow())
                .and(mGotIntentState)
                .subscribe(Observers.onEnter(Both.adapt((Window window, Intent intent) -> {
                    // Set flags to both exit sleep mode when this activity starts and
                    // avoid entering sleep mode while playing media. If an app that shouldn't turn
                    // on the screen is launching, we don't add TURN_SCREEN_ON.
                    if (CastWebContentsIntentUtils.shouldTurnOnScreen(intent)) turnScreenOn();
                    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                })));

        // Initialize the audio manager in onCreate() if tests haven't already.
        mCreatedState.and(Observable.not(mAudioManagerState)).subscribe(Observers.onEnter(x -> {
            mAudioManagerState.set(CastAudioManager.getAudioManager(this));
        }));

        // Clean up stream mute state on pause events.
        mAudioManagerState.andThen(Observable.not(mResumedState))
                .map(Both::getFirst)
                .subscribe(Observers.onEnter((CastAudioManager audioManager) -> {
                    audioManager.releaseStreamMuteIfNecessary(AudioManager.STREAM_MUSIC);
                }));

        // Handle each new Intent.
        Controller<CastWebContentsSurfaceHelper.StartParams> startParamsState = new Controller<>();
        mGotIntentState.and(Observable.not(mIsFinishingState))
                .map(Both::getFirst)
                .map(Intent::getExtras)
                .map(CastWebContentsSurfaceHelper.StartParams::fromBundle)
                // Use the duplicate-filtering functionality of Controller to drop duplicate params.
                .subscribe(Observers.onEnter(startParamsState::set));
        mSurfaceHelperState.and(startParamsState)
                .subscribe(Observers.onEnter(
                        Both.adapt(CastWebContentsSurfaceHelper::onNewStartParams)));

        mIsFinishingState.subscribe(Observers.onEnter((String reason) -> {
            if (DEBUG) Log.d(TAG, "Finishing activity: " + reason);
            mSurfaceHelperState.reset();
            finish();
        }));

        // If a new Intent arrives after finishing, start a new Activity instead of recycling this.
        gotIntentAfterFinishingState.subscribe(Observers.onEnter((Intent intent) -> {
            Log.d(TAG, "Got intent while finishing current activity, so start new activity.");
            int flags = intent.getFlags();
            flags = flags & ~Intent.FLAG_ACTIVITY_SINGLE_TOP;
            intent.setFlags(flags);
            startActivity(intent);
        }));

        Observable<?> stoppingBecauseUserLeftState =
                Observable.not(mStartedState).and(mUserLeftState);
        stoppingBecauseUserLeftState.subscribe(
                Observers.onEnter(x -> mIsFinishingState.set("User left and activity stopped.")));
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        if (DEBUG) Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);
        mCreatedState.set(Unit.unit());
        mGotIntentState.set(getIntent());

        // Whenever our app is visible, volume controls should modify the music stream.
        // For more information read:
        // http://developer.android.com/training/managing-audio/volume-playback.html
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (DEBUG) Log.d(TAG, "onNewIntent");
        mGotIntentState.set(intent);
    }

    @Override
    protected void onStart() {
        if (DEBUG) Log.d(TAG, "onStart");
        mStartedState.set(Unit.unit());
        super.onStart();
    }

    @Override
    protected void onPause() {
        if (DEBUG) Log.d(TAG, "onPause");
        super.onPause();
        mResumedState.reset();
    }

    @Override
    protected void onResume() {
        if (DEBUG) Log.d(TAG, "onResume");
        super.onResume();
        mResumedState.set(Unit.unit());
    }

    @Override
    protected void onStop() {
        if (DEBUG) Log.d(TAG, "onStop");
        mStartedState.reset();
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (DEBUG) Log.d(TAG, "onDestroy");
        mCreatedState.reset();
        super.onDestroy();
    }

    @Override
    protected void onUserLeaveHint() {
        mUserLeftState.set(Unit.unit());
        super.onUserLeaveHint();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (DEBUG) Log.d(TAG, "onWindowFocusChanged(%b)", hasFocus);
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            // switch to fullscreen (immersive) mode
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (mSurfaceHelper != null && mSurfaceHelper.isTouchInputEnabled()) {
            return super.dispatchTouchEvent(ev);
        } else {
            return false;
        }
    }

    private void turnScreenOn() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setTurnScreenOn(true);
        } else {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
        }
    }

    @RemovableInRelease
    public void finishForTesting() {
        mIsFinishingState.set("Finish for testing");
    }

    @RemovableInRelease
    public void testingModeForTesting() {
        mIsTestingState.set(Unit.unit());
    }

    @RemovableInRelease
    public void setAudioManagerForTesting(CastAudioManager audioManager) {
        mAudioManagerState.set(audioManager);
    }

    @RemovableInRelease
    public void setSurfaceHelperForTesting(CastWebContentsSurfaceHelper surfaceHelper) {
        mSurfaceHelperState.set(surfaceHelper);
    }
}
