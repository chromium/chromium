// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.animation.Animator;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.TextAppearanceSpan;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.VideoView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.math.MathUtils;
import androidx.core.view.GestureDetectorCompat;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Encapsulates the video player functionality of the Photo Picker dialog. */
public class PickerVideoPlayer extends FrameLayout
        implements View.OnClickListener,
                SeekBar.OnSeekBarChangeListener,
                View.OnSystemUiVisibilityChangeListener {
    /** A callback interface for notifying about video playback status. */
    public interface VideoPlaybackStatusCallback {
        // Called when the video starts playing.
        void onVideoPlaying();

        // Called when the video stops playing.
        void onVideoEnded();

        // Animation events for UI elements (views) fading in and out of view.
        void onAnimationStart(long viewId, float currentAlpha);

        void onAnimationCancel(long viewId, float currentAlpha);

        void onAnimationEnd(long viewId, float currentAlpha);
    }

    // The possible types of fade out animations.
    @IntDef({
        FadeOut.NO_FADE_OUT,
        FadeOut.FADE_OUT_PLAY_QUICKLY,
        FadeOut.FADE_OUT_ALL_SLOWLY,
        FadeOut.FADE_OUT_ALL_QUICKLY
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FadeOut {
        // This is used when the video controls should remain on screen and not fade away, for
        // example when the video playback reaches the end.
        int NO_FADE_OUT = 0;

        // This is used when the controls should fade out, with the Play button fading out faster
        // than the rest of the controls. This is appropriate to use when the goal is to get to
        // viewing a video quickly, for example when the video starts playing initially, or after
        // the user seeks during video playback.
        int FADE_OUT_PLAY_QUICKLY = 1;

        // This is used when all the controls, including the Play button, should fade out at the
        // same slow pace. This is appropriate to use when giving the user enough time to react
        // before the controls disappear again. For example, when the user has single-tapped on the
        // video to explicitly show the controls or when seeking while video playback is paused (due
        // to a high chance of the Play button being pressed).
        int FADE_OUT_ALL_SLOWLY = 2;

        // This is used when all the controls should fade out quickly, such as when the user single-
        // taps the video, to explicitly request that the controls disappear from view.
        int FADE_OUT_ALL_QUICKLY = 3;
    }

    // The callback to use for reporting playback progress in tests.
    private static VideoPlaybackStatusCallback sProgressCallback;

    // The amount of time (in milliseconds) to skip when fast forwarding/rewinding.
    private static final int SKIP_LENGTH_IN_MS = 10000;

    // The time (in milliseconds) to wait before animating controls away.
    private static final int OVERLAY_FADE_OUT_DELAY_MS = 2500;
    private static final int PLAY_BUTTON_FADE_OUT_DELAY_MS = 250;

    // Durations for fade-out animations (in milliseconds).
    private static final int PLAY_BUTTON_FADE_OUT_DURATION_MS = 750;
    private static final int OVERLAY_CONTROLS_FADE_OUT_DURATION_MS = 750;
    private static final int OVERLAY_SCRIM_FADE_OUT_DURATION_MS = 1000;

    // Durations for fade-in animation (in milliseconds).
    private static final int PLAY_BUTTON_FADE_IN_DURATION_MS = 250;
    private static final int OVERLAY_CONTROLS_FADE_IN_DURATION_MS = 500;
    private static final int OVERLAY_SCRIM_FADE_IN_DURATION_MS = 250;

    // Whether to turn on shorter animation timings and delays. When |true| all delays and
    // durations are 1/10th of normal length.
    private static boolean sShortAnimationTimesForTesting;

    // The Window for the dialog the player is shown in.
    private Window mWindow;

    // The Context to use.
    private Context mContext;

    // The Back button in the top corner.
    private final ImageView mBackButton;

    // The view showing the name of the video playing.
    private final TextView mFileName;

    // The video preview view.
    private final VideoView mVideoView;

    // The MediaPlayer object used to control the VideoView.
    private MediaPlayer mMediaPlayer;

    // The container view for all the UI elements overlaid on top of the video.
    private final View mVideoOverlayContainer;

    // Whether the overlay controls are currently showing. Set to true from the moment they start
    // animating into view and false once the Play/Pause button starts animating away.
    private boolean mOverlayControlsShowing;

    // The container view for the UI video controls within the overlaid window.
    private final View mVideoControls;

    // The scrim at the bottom of the video (highlighting the smaller video controls).
    private final View mVideoControlsGradient;

    // The large Play button overlaid on top of the video.
    private final ImageView mLargePlayButton;

    // The Mute button for the video.
    private final ImageView mMuteButton;

    // Keeps track of whether audio track is enabled or not.
    private boolean mAudioOn = true;

    // The Fullscreen button.
    private final ImageView mFullscreenButton;

    // Keeps track of whether full screen is enabled or not.
    private boolean mFullScreenEnabled;

    // Keeps track of whether full screen was toggled via the button in-app or via a system handled
    // user gesture (such as dragging from the top).
    private boolean mFullScreenToggledInApp;

    // The remaining video playback time.
    private final TextView mRemainingTime;

    // The message shown when seeking, to remind the user of the fast forward/back feature.
    private final LinearLayout mFastForwardMessage;

    // The SeekBar showing the video playback progress (allows user seeking).
    private final SeekBar mSeekBar;

    // Whether a seek operation happened while playback was taking place.
    private boolean mSeekDuringPlayback;

    // A flag to control when the playback monitor schedules new tasks.
    private boolean mRunPlaybackMonitoringTask;

    // The previous options for the System UI visibility.
    private int mPreviousSystemUiVisibilityOptions;

    // Keeps track of the previous navigation bar color when colors switch due to playback.
    private int mPreviousNavBarColor;

    // Keeps track of the previous navigation bar divider color when colors switch due to playback.
    private int mPreviousNavBarDividerColor;

    // Keeps track of whether navigation colors have been saved previously.
    private boolean mPreviousNavBarColorsSaved;

    // The object to convert touch events into gestures.
    private GestureDetectorCompat mGestureDetector;

    // An OnGestureListener class for handling double tap.
    private class DoubleTapGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onSingleTapConfirmed(MotionEvent e) {
            return onSingleTapVideo();
        }

        @Override
        public boolean onDoubleTap(MotionEvent e) {
            return onDoubleTapVideo(e.getX());
        }
    }

    /** Constructor for inflating from XML. */
    public PickerVideoPlayer(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        LayoutInflater.from(context).inflate(R.layout.video_player, this);

        mBackButton = findViewById(R.id.back_button);
        mFileName = findViewById(R.id.video_file_name);
        mVideoView = findViewById(R.id.video_player);
        mVideoOverlayContainer = findViewById(R.id.video_overlay_container);
        mVideoControls = findViewById(R.id.video_controls);
        mVideoControlsGradient = findViewById(R.id.video_controls_gradient);
        mLargePlayButton = findViewById(R.id.video_player_play_button);
        mMuteButton = findViewById(R.id.mute);
        mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
        mFullscreenButton = findViewById(R.id.fullscreen);
        mRemainingTime = findViewById(R.id.remaining_time);
        mSeekBar = findViewById(R.id.seek_bar);
        mFastForwardMessage = findViewById(R.id.fast_forward_message);

        mBackButton.setOnClickListener(this);
        mVideoOverlayContainer.setOnClickListener(this);
        mLargePlayButton.setOnClickListener(this);
        mMuteButton.setOnClickListener(this);
        mFullscreenButton.setOnClickListener(this);
        mSeekBar.setOnSeekBarChangeListener(this);

        mGestureDetector = new GestureDetectorCompat(context, new DoubleTapGestureListener());
        mVideoOverlayContainer.setOnTouchListener(
                new OnTouchListener() {
                    @Override
                    public boolean onTouch(View v, MotionEvent event) {
                        mGestureDetector.onTouchEvent(event);
                        return false;
                    }
                });
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // When configuration changes, the video size and controls need to be synced to the new
        // size. Post a task, so that size adjustments happen after layout of the video controls has
        // completed (so that the calculations for the view have had time to account for the
        // existence of the nav bar and status bar -- or lack thereof).
        getHandler().post(() -> adjustVideoLayoutParamsToOrientation());
        super.onConfigurationChanged(newConfig);
    }

    /**
     * Start playback of a video in an overlay above the photo picker.
     *
     * @param uri The uri of the video to start playing.
     * @param window The window for the dialog.
     */
    public void startVideoPlaybackAsync(Uri uri, Window window) {
        mWindow = window;
        syncNavBarColorToPlaybackStatus(/* playerOpening= */ true);

        // Make the filename (uri) of the video visible at the top and de-emphasize the scheme part.
        SpannableString fileName = new SpannableString(uri.toString());
        fileName.setSpan(
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMedium_Secondary),
                0,
                uri.getScheme().length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        mFileName.setText(fileName, TextView.BufferType.SPANNABLE);

        setVisibility(View.VISIBLE);

        mVideoView.setVisibility(View.VISIBLE);
        mVideoView.setVideoURI(uri);

        mVideoView.setOnPreparedListener(
                (MediaPlayer mediaPlayer) -> {
                    mMediaPlayer = mediaPlayer;
                    startVideoPlayback();

                    mMediaPlayer.setOnVideoSizeChangedListener(
                            (MediaPlayer player, int width, int height) -> {
                                adjustVideoLayoutParamsToOrientation();
                                mVideoOverlayContainer.setVisibility(View.VISIBLE);
                            });

                    if (sProgressCallback != null) {
                        mMediaPlayer.setOnInfoListener(
                                (MediaPlayer player, int what, int extra) -> {
                                    if (what == MediaPlayer.MEDIA_INFO_VIDEO_RENDERING_START) {
                                        sProgressCallback.onVideoPlaying();
                                        return true;
                                    }
                                    return false;
                                });
                    }
                });

        mVideoView.setOnCompletionListener(
                new MediaPlayer.OnCompletionListener() {
                    @Override
                    public void onCompletion(MediaPlayer mediaPlayer) {
                        // Once we reach the completion point, show the overlay controls (without
                        // fading away) to indicate that playback has reached the end of the video
                        // (and didn't break before reaching the end). This also allows the user
                        // to restart playback from the start, by pressing Play.
                        switchToPlayButton();
                        updateProgress();
                        showAndMaybeHideVideoControls(/* animateIn= */ false, FadeOut.NO_FADE_OUT);
                        if (sProgressCallback != null) {
                            sProgressCallback.onVideoEnded();
                        }
                    }
                });
    }

    /**
     * Ends video playback (if a video is playing) and closes the video player. Aborts if the video
     * playback container is not showing.
     *
     * @return true if a video container was showing, false otherwise.
     */
    public boolean closeVideoPlayer() {
        if (getVisibility() != View.VISIBLE) {
            return false;
        }

        setVisibility(View.GONE);
        stopVideoPlayback();
        mVideoView.setMediaController(null);
        mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
        syncNavBarColorToPlaybackStatus(/* playerOpening= */ false);
        return true;
    }

    /**
     * Updates the color of the navigation bar, divider and icons to reflect whether in playback
     * mode or not. This function does nothing on Android O and older.
     *
     * @param playerOpening True when the video player is opening (false when closing).
     */
    private void syncNavBarColorToPlaybackStatus(boolean playerOpening) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            if (playerOpening) {
                if (mPreviousNavBarColorsSaved) {
                    return; // Don't overwrite previously saved colors.
                }
                mPreviousNavBarColor = mWindow.getNavigationBarColor();
                mPreviousNavBarDividerColor = mWindow.getNavigationBarDividerColor();
            }
            mWindow.setNavigationBarColor(playerOpening ? Color.BLACK : mPreviousNavBarColor);
            mWindow.setNavigationBarDividerColor(
                    playerOpening ? Color.BLACK : mPreviousNavBarDividerColor);
            UiUtils.setNavigationBarIconColor(mWindow.getDecorView().getRootView(), !playerOpening);

            mPreviousNavBarColorsSaved = playerOpening;
        }
    }

    private void adjustVideoLayoutParamsToOrientation() {
        if (mMediaPlayer == null
                || mMediaPlayer.getVideoWidth() == 0
                || mMediaPlayer.getVideoHeight() == 0) {
            return;
        }
        float aspectRatio = (float) mMediaPlayer.getVideoWidth() / mMediaPlayer.getVideoHeight();

        boolean landscapeMode =
                mContext.getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE;
        int viewWidth =
                landscapeMode
                        ? Math.max(getWidth(), getHeight())
                        : Math.min(getWidth(), getHeight());
        int viewHeight =
                landscapeMode
                        ? Math.min(getWidth(), getHeight())
                        : Math.max(getWidth(), getHeight());

        ViewGroup.LayoutParams layoutParams = mVideoView.getLayoutParams();
        if (landscapeMode) {
            // Landscape mode. Use full height of the container.
            layoutParams.width = Math.round(viewHeight * aspectRatio);
            layoutParams.height = viewHeight;

            // Check if there's enough width to show all the video. If not, use full view width
            // instead with aspect ratio of the video (black bars appear above and below the video).
            if (layoutParams.width > viewWidth) {
                layoutParams.width = viewWidth;
                layoutParams.height = Math.round(viewWidth / aspectRatio);
            }

            // In landscape mode, the video will obscure parts or all of these.
            mBackButton.setVisibility(View.GONE);
            mFileName.setVisibility(View.GONE);
        } else {
            // Portrait mode. Use full width of the container.
            layoutParams.height = Math.round(viewWidth / aspectRatio);
            layoutParams.width = viewWidth;

            // Check if there's enough height to show all the video. If not, use full view height
            // instead with aspect ratio of the video (black bars appear left and right).
            if (layoutParams.height > viewHeight) {
                layoutParams.height = viewHeight;
                layoutParams.width = Math.round(viewHeight * aspectRatio);
            }

            mBackButton.setVisibility(View.VISIBLE);
            mFileName.setVisibility(View.VISIBLE);
        }

        mVideoView.setLayoutParams(layoutParams);
        ViewUtils.requestLayout(
                mVideoView, "PickerVideoPlayer.adjustVideoLayoutParamsToOrientation mVideoView");
        mVideoControls.setLayoutParams(layoutParams);
        ViewUtils.requestLayout(
                mVideoControls,
                "PickerVideoPlayer.adjustVideoLayoutParamsToOrientation mVideoControls");
    }

    private boolean onSingleTapVideo() {
        if (mOverlayControlsShowing) {
            // A tap when overlays are showing is treated as a request for the controls to
            // disappear as soon as possible.
            fadeAwayVideoControls(FadeOut.FADE_OUT_ALL_QUICKLY);
        } else {
            // A tap when the overlay controls are hidden should be treated as a high likelihood of
            // the user wanting to interact with the controls, so they should remain on screen
            // longer.
            showAndMaybeHideVideoControls(/* animateIn= */ true, FadeOut.FADE_OUT_ALL_SLOWLY);
        }
        return true;
    }

    private boolean onDoubleTapVideo(float x) {
        int videoPos = mMediaPlayer.getCurrentPosition();
        int duration = mMediaPlayer.getDuration();

        // A click to the left (of the center of) the Play button counts as rewinding, and a click
        // to the right of it counts as fast forwarding.
        float midX = mLargePlayButton.getX() + (mLargePlayButton.getWidth() / 2f);
        videoPos += (x > midX) ? SKIP_LENGTH_IN_MS : -SKIP_LENGTH_IN_MS;
        MathUtils.clamp(videoPos, 0, duration);

        videoSeekTo(videoPos);
        updateProgress();
        showAndMaybeHideVideoControls(/* animateIn= */ false, FadeOut.FADE_OUT_PLAY_QUICKLY);
        return true;
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.video_player_play_button) {
            toggleVideoPlayback();
        } else if (id == R.id.back_button) {
            closeVideoPlayer();
        } else if (id == R.id.mute) {
            toggleMute();
        } else if (id == R.id.fullscreen) {
            toggleAndroidSystemUiForFullscreen();
        }
    }

    // View.OnSystemUiVisibilityChangeListener:

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
            mWindow.getDecorView().setOnSystemUiVisibilityChangeListener(null);
            onExitFullScreenMode();

            if (!mFullScreenToggledInApp) {
                // When the user drops out of full screen via a system gesture, such as dragging
                // from the top of the screen, the system sends the visibility change event before
                // the resize has happened, so the new video size isn't known yet. Syncing
                // immediately would make the overlay controls appear in the wrong location.
                getHandler().post(() -> adjustVideoLayoutParamsToOrientation());
                return;
            }
        } else {
            onEnterFullScreenMode();
        }

        adjustVideoLayoutParamsToOrientation();
        mFullScreenToggledInApp = false;
    }

    // SeekBar.OnSeekBarChangeListener:

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            float percentage = progress / 100f;
            int position = Math.round(percentage * mVideoView.getDuration());
            videoSeekTo(position);
            updateProgress();
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        showAndMaybeHideVideoControls(/* animateIn= */ false, FadeOut.NO_FADE_OUT);
        if (mVideoView.isPlaying()) {
            stopVideoPlayback();
            mSeekDuringPlayback = true;
        }
        mFastForwardMessage.setVisibility(View.VISIBLE);
        mLargePlayButton.setVisibility(View.GONE);
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        fadeAwayVideoControls(
                mSeekDuringPlayback ? FadeOut.FADE_OUT_PLAY_QUICKLY : FadeOut.FADE_OUT_ALL_SLOWLY);
        if (mSeekDuringPlayback) {
            startVideoPlayback();
            mSeekDuringPlayback = false;
        }
        mFastForwardMessage.setVisibility(View.GONE);
        mLargePlayButton.setVisibility(View.VISIBLE);
    }

    private void videoSeekTo(int position) {
        if (Build.VERSION.SDK_INT >= 26) {
            mMediaPlayer.seekTo(position, MediaPlayer.SEEK_CLOSEST);
        } else {
            // On older versions, sync to nearest previous key frame.
            mVideoView.seekTo(position);
        }
    }

    private int scaledTiming(int timespan) {
        return sShortAnimationTimesForTesting ? timespan / 10 : timespan;
    }

    private void fadeAwayVideoControls(@FadeOut int fadeOutType) {
        if (fadeOutType == FadeOut.NO_FADE_OUT) {
            return;
        }

        mVideoControls.animate().cancel();
        mVideoControlsGradient.animate().cancel();
        mLargePlayButton.animate().cancel();

        int delay = fadeOutType != FadeOut.FADE_OUT_ALL_QUICKLY ? OVERLAY_FADE_OUT_DELAY_MS : 0;
        mVideoControlsGradient
                .animate()
                .alpha(0.0f)
                .setStartDelay(scaledTiming(delay))
                .setDuration(scaledTiming(OVERLAY_SCRIM_FADE_OUT_DURATION_MS));

        mVideoControls
                .animate()
                .alpha(0.0f)
                .setStartDelay(scaledTiming(delay))
                .setDuration(scaledTiming(OVERLAY_CONTROLS_FADE_OUT_DURATION_MS))
                .setListener(
                        new Animator.AnimatorListener() {
                            @Override
                            public void onAnimationStart(Animator animation) {
                                notifyTestOfAnimationStart(mVideoControls);
                            }

                            @Override
                            public void onAnimationEnd(Animator animation) {
                                enableClickableButtons(false);
                                stopPlaybackMonitor();

                                notifyTestOfAnimationEnd(mVideoControls);
                            }

                            @Override
                            public void onAnimationCancel(Animator animation) {
                                notifyTestOfAnimationCancel(mVideoControls);
                            }

                            @Override
                            public void onAnimationRepeat(Animator animation) {}
                        });

        int animationDelay = 0;
        if (fadeOutType != FadeOut.FADE_OUT_ALL_QUICKLY) {
            animationDelay =
                    fadeOutType == FadeOut.FADE_OUT_PLAY_QUICKLY
                            ? PLAY_BUTTON_FADE_OUT_DELAY_MS
                            : OVERLAY_FADE_OUT_DELAY_MS;
        }

        mLargePlayButton
                .animate()
                .alpha(0.0f)
                .setStartDelay(scaledTiming(animationDelay))
                .setDuration(scaledTiming(PLAY_BUTTON_FADE_OUT_DURATION_MS))
                .setListener(
                        new Animator.AnimatorListener() {
                            @Override
                            public void onAnimationStart(Animator animation) {
                                // The Play button is always the first control to fade away, and any
                                // click after that point should be considered a request to cancel
                                // fading away.
                                // Therefore this is a good time to flip this to false.
                                mOverlayControlsShowing = false;

                                notifyTestOfAnimationStart(mLargePlayButton);
                            }

                            @Override
                            public void onAnimationEnd(Animator animation) {
                                mLargePlayButton.setClickable(false);

                                notifyTestOfAnimationEnd(mLargePlayButton);
                            }

                            @Override
                            public void onAnimationCancel(Animator animation) {
                                notifyTestOfAnimationCancel(mLargePlayButton);
                            }

                            @Override
                            public void onAnimationRepeat(Animator animation) {}
                        });
    }

    /**
     * Shows video controls overlaid on top of the video. The controls can optionally be faded in
     * and out of view.
     *
     * @param animateIn True if the overlay controls should animate into view.
     * @param fadeOutType Whether and how to animate the controls out of view. If fadeOutType is
     *     NO_FADE_OUT the controls will remain on screen once the function is done.
     */
    private void showAndMaybeHideVideoControls(boolean animateIn, @FadeOut int fadeOutType) {
        mVideoControls.animate().cancel();
        mVideoControlsGradient.animate().cancel();
        mLargePlayButton.animate().cancel();

        if (mVideoView.isPlaying()) {
            startPlaybackMonitor();
        }

        mOverlayControlsShowing = true;
        if (!animateIn) {
            mVideoControls.setAlpha(1.0f);
            mVideoControlsGradient.setAlpha(1.0f);
            mLargePlayButton.setAlpha(1.0f);

            enableClickableButtons(true);
            mLargePlayButton.setClickable(true);
            fadeAwayVideoControls(fadeOutType);
        } else {
            mVideoControlsGradient
                    .animate()
                    .alpha(1.0f)
                    .setStartDelay(0)
                    .setDuration(scaledTiming(OVERLAY_SCRIM_FADE_IN_DURATION_MS));

            mVideoControls
                    .animate()
                    .alpha(1.0f)
                    .setStartDelay(0)
                    .setDuration(scaledTiming(OVERLAY_CONTROLS_FADE_IN_DURATION_MS))
                    .setListener(
                            new Animator.AnimatorListener() {
                                @Override
                                public void onAnimationStart(Animator animation) {
                                    notifyTestOfAnimationStart(mVideoControls);
                                }

                                @Override
                                public void onAnimationEnd(Animator animation) {
                                    enableClickableButtons(true);
                                    // After animating the controls into view, start a timer for
                                    // fading them out again, if needed.
                                    fadeAwayVideoControls(fadeOutType);

                                    notifyTestOfAnimationEnd(mVideoControls);
                                }

                                @Override
                                public void onAnimationCancel(Animator animation) {
                                    notifyTestOfAnimationCancel(mVideoControls);
                                }

                                @Override
                                public void onAnimationRepeat(Animator animation) {}
                            });

            mLargePlayButton
                    .animate()
                    .alpha(1.0f)
                    .setStartDelay(0)
                    .setDuration(scaledTiming(PLAY_BUTTON_FADE_IN_DURATION_MS))
                    .setListener(
                            new Animator.AnimatorListener() {
                                @Override
                                public void onAnimationStart(Animator animation) {
                                    notifyTestOfAnimationStart(mLargePlayButton);
                                }

                                @Override
                                public void onAnimationEnd(Animator animation) {
                                    mLargePlayButton.setClickable(true);

                                    notifyTestOfAnimationEnd(mLargePlayButton);
                                }

                                @Override
                                public void onAnimationCancel(Animator animation) {
                                    notifyTestOfAnimationCancel(mLargePlayButton);
                                }

                                @Override
                                public void onAnimationRepeat(Animator animation) {}
                            });
        }
    }

    private void enableClickableButtons(boolean enable) {
        mMuteButton.setClickable(enable);
        mFullscreenButton.setClickable(enable);
    }

    private void updateProgress() {
        String current;
        String total;
        try {
            current = DecodeVideoTask.formatDuration(Long.valueOf(mVideoView.getCurrentPosition()));
            total = DecodeVideoTask.formatDuration(Long.valueOf(mVideoView.getDuration()));
        } catch (IllegalStateException exception) {
            // VideoView#getCurrentPosition throws this error if the dialog has been dismissed while
            // waiting to update the status.
            return;
        }

        String formattedProgress =
                mContext.getResources()
                        .getString(R.string.photo_picker_video_duration, current, total);
        mRemainingTime.setText(formattedProgress);
        mRemainingTime.setContentDescription(
                mContext.getResources()
                        .getString(R.string.accessibility_playback_time, current, total));
        int percentage =
                mVideoView.getDuration() == 0
                        ? 0
                        : mVideoView.getCurrentPosition() * 100 / mVideoView.getDuration();
        mSeekBar.setProgress(percentage);

        if (mVideoView.isPlaying() && mRunPlaybackMonitoringTask) {
            startPlaybackMonitor();
        }
    }

    private void startVideoPlayback() {
        mMediaPlayer.start();
        switchToPauseButton();
        showAndMaybeHideVideoControls(/* animateIn= */ false, FadeOut.FADE_OUT_PLAY_QUICKLY);
    }

    private void stopVideoPlayback() {
        stopPlaybackMonitor();

        mMediaPlayer.pause();
        switchToPlayButton();
        showAndMaybeHideVideoControls(/* animateIn= */ false, FadeOut.NO_FADE_OUT);
    }

    private void toggleVideoPlayback() {
        if (mVideoView.isPlaying()) {
            stopVideoPlayback();
        } else {
            startVideoPlayback();
        }
    }

    private void switchToPlayButton() {
        mLargePlayButton.setImageResource(R.drawable.ic_play_circle_filled_white_24dp);
        mLargePlayButton.setContentDescription(
                mContext.getResources().getString(R.string.accessibility_play_video));
    }

    private void switchToPauseButton() {
        mLargePlayButton.setImageResource(R.drawable.ic_pause_circle_outline_white_24dp);
        mLargePlayButton.setContentDescription(
                mContext.getResources().getString(R.string.accessibility_pause_video));
    }

    private void toggleMute() {
        mAudioOn = !mAudioOn;
        if (mAudioOn) {
            mMediaPlayer.setVolume(1f, 1f);
            mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
            mMuteButton.setContentDescription(
                    mContext.getResources().getString(R.string.accessibility_mute_video));
        } else {
            mMediaPlayer.setVolume(0f, 0f);
            mMuteButton.setImageResource(R.drawable.ic_volume_off_white_24dp);
            mMuteButton.setContentDescription(
                    mContext.getResources().getString(R.string.accessibility_unmute_video));
        }
    }

    private void onEnterFullScreenMode() {
        assert !mFullScreenEnabled;
        mFullscreenButton.setImageResource(R.drawable.ic_full_screen_exit_white_24dp);
        mFullscreenButton.setContentDescription(
                mContext.getResources().getString(R.string.accessibility_exit_full_screen));
        mFullScreenEnabled = true;
    }

    private void onExitFullScreenMode() {
        assert mFullScreenEnabled;
        mFullscreenButton.setImageResource(R.drawable.ic_full_screen_white_24dp);
        mFullscreenButton.setContentDescription(
                mContext.getResources().getString(R.string.accessibility_full_screen));
        mFullScreenEnabled = false;
    }

    private void toggleAndroidSystemUiForFullscreen() {
        mFullScreenToggledInApp = true;
        View decorView = mWindow.getDecorView();
        if (!mFullScreenEnabled) {
            decorView.setOnSystemUiVisibilityChangeListener(this);
            mPreviousSystemUiVisibilityOptions = decorView.getSystemUiVisibility();
            decorView.setSystemUiVisibility(
                    mPreviousSystemUiVisibilityOptions
                            | View.SYSTEM_UI_FLAG_IMMERSIVE
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LOW_PROFILE);
        } else {
            decorView.setSystemUiVisibility(mPreviousSystemUiVisibilityOptions);
        }

        // Calling setSystemUiVisibility will result in Android showing/hiding its system UI to go
        // into or out of full screen mode. This happens asynchronously and once that change is
        // complete the video player needs to respond to those changes. Flow therefore continues in
        // onSystemUiVisibilityChange, which the system calls when it is done.
    }

    private void startPlaybackMonitor() {
        mRunPlaybackMonitoringTask = true;
        startPlaybackMonitorTask();
    }

    private void startPlaybackMonitorTask() {
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, () -> updateProgress(), 250);
    }

    private void stopPlaybackMonitor() {
        mRunPlaybackMonitoringTask = false;
    }

    /** Sets the video playback progress callback. For testing use only. */
    @VisibleForTesting
    public static void setProgressCallback(VideoPlaybackStatusCallback callback) {
        sProgressCallback = callback;
    }

    /** Sets whether to use shorter timeouts and durations. For testing use only. */
    public static void setShortAnimationTimesForTesting(boolean value) {
        sShortAnimationTimesForTesting = value;
        ResettersForTesting.register(() -> sShortAnimationTimesForTesting = false);
    }

    public void singleTapForTesting() {
        onSingleTapVideo();
    }

    public void doubleTapForTesting(float x) {
        onDoubleTapVideo(x);
    }

    public void notifyTestOfAnimationStart(View view) {
        if (sProgressCallback != null) {
            sProgressCallback.onAnimationStart(view.getId(), view.getAlpha());
        }
    }

    public void notifyTestOfAnimationEnd(View view) {
        if (sProgressCallback != null) {
            sProgressCallback.onAnimationEnd(view.getId(), view.getAlpha());
        }
    }

    public void notifyTestOfAnimationCancel(View view) {
        if (sProgressCallback != null) {
            sProgressCallback.onAnimationCancel(view.getId(), view.getAlpha());
        }
    }
}
