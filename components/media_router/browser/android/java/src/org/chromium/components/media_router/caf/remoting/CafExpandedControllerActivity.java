// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.media.session.PlaybackStateCompat;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;
import androidx.mediarouter.app.MediaRouteButton;

import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.media_router.R;
import org.chromium.components.media_router.caf.BaseSessionController;
import org.chromium.third_party.android.media.MediaController;

/** The activity that's opened by clicking the video flinging (casting) notification. */
@NullMarked
@SuppressWarnings("NullAway") // https://crbug.com/401584051
public class CafExpandedControllerActivity extends FragmentActivity
        implements BaseSessionController.Callback {
    private static final int PROGRESS_UPDATE_PERIOD_IN_MS = 1000;

    private Handler mHandler;
    // We don't use the standard android.media.MediaController, but a custom one.
    // See the class itself for details.
    private MediaController mMediaController;
    private RemotingSessionController mSessionController;
    private @Nullable MediaRouteButton mMediaRouteButton;
    private TextView mTitleView;
    private Runnable mUpdateProgressRunnable;

    private RemoteMediaClient getRemoteMediaClient() {
        RemoteMediaClient ret =
                assumeNonNull(mSessionController.getSession()).getRemoteMediaClient();
        assert ret != null;
        return ret;
    }

    /** Handle actions from on-screen media controls. */
    private final MediaController.Delegate mControllerDelegate =
            new MediaController.Delegate() {
                @Override
                public void play() {
                    if (!mSessionController.isConnected()) return;

                    getRemoteMediaClient().play();
                }

                @Override
                public void pause() {
                    if (!mSessionController.isConnected()) return;

                    getRemoteMediaClient().pause();
                }

                @Override
                public long getDuration() {
                    if (!mSessionController.isConnected()) return 0;
                    return assumeNonNull(mSessionController.getFlingingController()).getDuration();
                }

                @Override
                public long getPosition() {
                    if (!mSessionController.isConnected()) return 0;
                    return assumeNonNull(mSessionController.getFlingingController())
                            .getApproximateCurrentTime();
                }

                @Override
                public void seekTo(long pos) {
                    if (!mSessionController.isConnected()) return;

                    getRemoteMediaClient().seek(pos);
                }

                @Override
                public boolean isPlaying() {
                    if (!mSessionController.isConnected()) return false;

                    return getRemoteMediaClient().isPlaying();
                }

                @Override
                public long getActionFlags() {
                    long flags =
                            PlaybackStateCompat.ACTION_REWIND
                                    | PlaybackStateCompat.ACTION_FAST_FORWARD;
                    if (mSessionController.isConnected() && getRemoteMediaClient().isPlaying()) {
                        flags |= PlaybackStateCompat.ACTION_PAUSE;
                    } else {
                        flags |= PlaybackStateCompat.ACTION_PLAY;
                    }
                    return flags;
                }
            };

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        RemotingSessionController sessionController = RemotingSessionController.getInstance();

        if (sessionController == null || !sessionController.isConnected()) {
            // Suppress NullAway warnings about these fields being left null.
            assumeNonNull(mHandler);
            assumeNonNull(mMediaController);
            assumeNonNull(mSessionController);
            assumeNonNull(mTitleView);
            assumeNonNull(mUpdateProgressRunnable);

            finish();
            return;
        }

        mSessionController = sessionController;
        sessionController.addCallback(this);

        // Make the activity full screen.
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow()
                .setFlags(
                        WindowManager.LayoutParams.FLAG_FULLSCREEN,
                        WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // requestWindowFeature must be called before adding content.
        setContentView(R.layout.expanded_cast_controller);

        ViewGroup rootView = (ViewGroup) findViewById(android.R.id.content);
        rootView.setBackgroundColor(Color.BLACK);

        // Create and initialize the media control UI.
        mMediaController = (MediaController) findViewById(R.id.cast_media_controller);
        mMediaController.setDelegate(mControllerDelegate);

        View castButtonView =
                getLayoutInflater()
                        .inflate(R.layout.caf_controller_media_route_button, rootView, false);
        if (castButtonView instanceof MediaRouteButton) {
            mMediaRouteButton = (MediaRouteButton) castButtonView;
            rootView.addView(mMediaRouteButton);
            mMediaRouteButton.bringToFront();
            var routeSelector = assumeNonNull(mSessionController.getSource()).buildRouteSelector();
            assert routeSelector != null;
            mMediaRouteButton.setRouteSelector(routeSelector);
        }

        mTitleView = (TextView) findViewById(R.id.cast_screen_title);

        mHandler = new Handler();
        mUpdateProgressRunnable = this::updateProgress;

        updateUi();
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (mSessionController == null || !mSessionController.isConnected()) {
            finish();
            return;
        }
    }

    @Override
    protected void onDestroy() {
        mSessionController.removeCallback(this);
        super.onDestroy();
    }

    @Override
    public void onSessionStarted() {}

    @Override
    public void onSessionEnded() {
        finish();
    }

    @Override
    public void onStatusUpdated() {
        updateUi();
    }

    @Override
    public void onMetadataUpdated() {
        updateUi();
    }

    private void updateUi() {
        if (!mSessionController.isConnected()) return;

        String deviceName =
                assumeNonNull(assumeNonNull(mSessionController.getSession()).getCastDevice())
                        .getFriendlyName();
        String titleText = "";
        if (deviceName != null) {
            titleText = getString(R.string.cast_casting_video, deviceName);
        }
        mTitleView.setText(titleText);

        mMediaController.refresh();
        mMediaController.updateProgress();

        cancelProgressUpdateTask();
        if (getRemoteMediaClient().isPlaying()) {
            scheduleProgressUpdateTask();
        }
    }

    private void scheduleProgressUpdateTask() {
        mHandler.postDelayed(mUpdateProgressRunnable, PROGRESS_UPDATE_PERIOD_IN_MS);
    }

    private void cancelProgressUpdateTask() {
        mHandler.removeCallbacks(mUpdateProgressRunnable);
    }

    private void updateProgress() {
        mMediaController.updateProgress();
        scheduleProgressUpdateTask();
    }
}
