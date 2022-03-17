// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.PictureInPictureParams;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.chromium.base.Log;
import org.chromium.chromecast.base.Both;
import org.chromium.chromecast.base.CastSwitches;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Observers;
import org.chromium.chromecast.base.Unit;
import org.chromium.content_public.browser.WebContents;

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

    // JavaScript to execute on WebContents when the back key is pressed.
    // This will return a value that indicates whether or not the default
    // Android behavior for the back key should be disabled or not.
    private static final String BACK_PRESSED_JAVASCRIPT = "{"
            + "  let getActiveElement = function() {"
            + "    let activeElement = document.activeElement;"
            + "    while (activeElement && activeElement.shadowRoot && activeElement.shadowRoot.activeElement) {"
            + "      activeElement = activeElement.shadowRoot.activeElement;"
            + "    }"
            + "    return activeElement;"
            + "  };"
            + "  let backPressEvent = new KeyboardEvent("
            + "     \"keydown\", {"
            + "      bubbles: true,"
            + "      key: \"BrowserBack\","
            + "      cancelable: true,"
            + "      composed: true"
            + "     }"
            + "  );"
            + "  let activeElement = getActiveElement();"
            + "  if (activeElement) {"
            + "    activeElement.dispatchEvent(backPressEvent);"
            + "  } else {"
            + "    document.dispatchEvent(backPressEvent);"
            + "  }"
            + "  backPressEvent.defaultPrevented;"
            + "};";

    // Tracks whether this Activity is between onCreate() and onDestroy().
    private final Controller<Unit> mCreatedState = new Controller<>();
    // Tracks whether this Activity is between onStart() and onStop().
    private final Controller<Unit> mStartedState = new Controller<>();
    // Tracks the most recent Intent for the Activity.
    private final Controller<Intent> mGotIntentState = new Controller<>();
    // Set this to cause the Activity to finish.
    private final Controller<String> mIsFinishingState = new Controller<>();
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
            CastBrowserHelper.initializeBrowser(getApplicationContext());

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

        // Set a flag to exit sleep mode when this activity starts.
        mCreatedState.and(mGotIntentState)
                .map(Both::getSecond)
                // Turn the screen on only if the launching Intent asks to.
                .filter(CastWebContentsIntentUtils::shouldTurnOnScreen)
                .subscribe(Observers.onEnter(x -> turnScreenOn()));

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
            finishAndRemoveTask();
        }));

        mStartedState.subscribe(x -> {
            Context ctx = getApplicationContext();
            Intent intent = getIntent();
            String instanceId = CastWebContentsIntentUtils.getSessionId(intent.getExtras());
            Intent visible = CastWebContentsIntentUtils.onVisibilityChange(
                    instanceId, CastWebContentsIntentUtils.VISIBITY_TYPE_FULL_SCREEN);
            LocalBroadcastManager.getInstance(ctx).sendBroadcastSync(visible);
            return () -> {
                Intent hidden = CastWebContentsIntentUtils.onVisibilityChange(
                        instanceId, CastWebContentsIntentUtils.VISIBITY_TYPE_HIDDEN);
                LocalBroadcastManager.getInstance(ctx).sendBroadcastSync(hidden);
            };
        });

        // If a new Intent arrives after finishing, start a new Activity instead of recycling this.
        gotIntentAfterFinishingState.subscribe(Observers.onEnter((Intent intent) -> {
            Log.d(TAG, "Got intent while finishing current activity, so start new activity.");
            int flags = intent.getFlags();
            flags = flags & ~Intent.FLAG_ACTIVITY_SINGLE_TOP;
            intent.setFlags(flags);
            startActivity(intent);
        }));
    }

    @RequiresApi(Build.VERSION_CODES.S)
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

        if (canAutoEnterPictureInPicture()) {
            setPictureInPictureParams(
                    new PictureInPictureParams.Builder().setAutoEnterEnabled(true).build());
        }
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
    protected void onStop() {
        if (DEBUG) Log.d(TAG, "onStop");
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
        if (DEBUG) Log.d(TAG, "onDestroy");

        mCreatedState.reset();
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        WebContents webContents = CastWebContentsIntentUtils.getWebContents(getIntent());
        if (webContents == null) {
            super.onBackPressed();
            return;
        }
        webContents.evaluateJavaScript(BACK_PRESSED_JAVASCRIPT, defaultPrevented -> {
            if (!"true".equals(defaultPrevented)) {
                super.onBackPressed();
            }
        });
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

    private static boolean isInLockTaskMode(Context context) {
        ActivityManager activityManager = context.getSystemService(ActivityManager.class);
        return activityManager.getLockTaskModeState() != ActivityManager.LOCK_TASK_MODE_NONE;
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    public void onUserLeaveHint() {
        if (DEBUG) Log.d(TAG, "onUserLeaveHint");
        if (canUsePictureInPicture() && !canAutoEnterPictureInPicture()) {
            enterPictureInPictureMode(new PictureInPictureParams.Builder().build());
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
                && getPackageManager().hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE);
    }

    private boolean canAutoEnterPictureInPicture() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && getPackageManager().hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE);
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
