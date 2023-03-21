// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo.State;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialUtils;
import org.chromium.chrome.browser.video_tutorials.languages.LanguagePickerCoordinator;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.LanguagePickerAction;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.UserAction;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.WatchState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator for the video player UI, responsible for changing the state of UI based on user
 * interaction events, and player state.
 */
class VideoPlayerMediator implements PlaybackStateObserver.Observer {
    private static final String VARIATION_ENABLE_SHARE_BUTTON = "enable_share";
    public static Boolean sEnableShareForTesting;

    private final Context mContext;
    private final VideoTutorialService mVideoTutorialService;
    private final PropertyModel mModel;
    private final LanguagePickerCoordinator mLanguagePicker;
    private final WebContents mWebContents;
    private Tutorial mTutorial;
    private final Callback<Tutorial> mTryNowCallback;
    private final Runnable mCloseCallback;
    private final PlaybackStateObserver mPlaybackStateObserver;
    private long mVideoStartTime;

    /** Constructor. */
    public VideoPlayerMediator(Context context, PropertyModel model,
            VideoTutorialService videoTutorialService, LanguagePickerCoordinator languagePicker,
            WebContents webContents, PlaybackStateObserver playbackStateObserver,
            Callback<Tutorial> tryNowCallback, Runnable closeCallback) {
        mContext = context;
        mModel = model;
        mVideoTutorialService = videoTutorialService;
        mLanguagePicker = languagePicker;
        mWebContents = webContents;
        mTryNowCallback = tryNowCallback;
        mCloseCallback = closeCallback;
        mPlaybackStateObserver = playbackStateObserver;

        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, false);
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, false);
        hideMediaControls();
        mModel.set(VideoPlayerProperties.CALLBACK_WATCH_NEXT, this::onWatchNextClicked);
        mModel.set(VideoPlayerProperties.CALLBACK_CHANGE_LANGUAGE, this::changeLanguage);
        mModel.set(VideoPlayerProperties.CALLBACK_TRY_NOW, this::tryNow);
        mModel.set(VideoPlayerProperties.CALLBACK_SHARE, this::share);
        mModel.set(VideoPlayerProperties.CALLBACK_CLOSE, this::close);
        mModel.set(VideoPlayerProperties.CALLBACK_PLAY_BUTTON, () -> startVideo(mTutorial));
    }

    /** Called when the player is getting destroyed. */
    public void destroy() {
        if (mPlaybackStateObserver.getWatchStateInfo().videoWatched()) {
            VideoTutorialMetrics.recordWatchStateUpdate(mTutorial.featureType, WatchState.WATCHED);
        }
    }

    boolean handleBackPressed() {
        // TODO(crbug.com/1406012): Remove these metrics or introduce new metrics in other lifecycle
        //                          hooks because this method never consumes back event.
        boolean isShowingLanguagePicker = mModel.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER);
        boolean isShowingLoadingScreen = mModel.get(VideoPlayerProperties.SHOW_LOADING_SCREEN);
        boolean isShowingVideoPlayer = !isShowingLanguagePicker && !isShowingLoadingScreen;

        if (isShowingVideoPlayer) {
            VideoTutorialMetrics.recordUserAction(
                    mTutorial.featureType, UserAction.BACK_PRESS_WHEN_SHOWING_VIDEO_PLAYER);
        } else if (isShowingLanguagePicker) {
            VideoTutorialMetrics.recordLanguagePickerAction(LanguagePickerAction.BACK_PRESS);
        }

        return false;
    }

    /**
     * Entry point for playing a tutorial video. Shows the language picker if it is the very first
     * time.
     */
    void playVideoTutorial(Tutorial tutorial) {
        mTutorial = tutorial;
        boolean shouldShowLanguagePicker =
                TextUtils.isEmpty(mVideoTutorialService.getPreferredLocale())
                && areMultipleLanguagesAvailable();
        if (shouldShowLanguagePicker) {
            mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, true);
            mLanguagePicker.showLanguagePicker(
                    mTutorial.featureType, this::onLanguageSelected, mCloseCallback);
        } else {
            startVideo(tutorial);
        }
    }

    @Override
    public void onPlay() {
        VideoTutorialMetrics.recordWatchStateUpdate(mTutorial.featureType, WatchState.RESUMED);
        if (mVideoStartTime != 0) {
            VideoTutorialMetrics.recordVideoLoadTimeLatency(
                    System.currentTimeMillis() - mVideoStartTime);
            // Set it to zero to ignore subsequent pause/resume events.
            mVideoStartTime = 0;
        }

        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, false);
        hideMediaControls();
    }

    @Override
    public void onPause() {
        VideoTutorialMetrics.recordWatchStateUpdate(mTutorial.featureType, WatchState.PAUSED);
        mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, false);
        mModel.set(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE, false);
        mModel.set(VideoPlayerProperties.SHOW_TRY_NOW,
                VideoTutorialUtils.shouldShowTryNow(mTutorial.featureType));
        mModel.set(VideoPlayerProperties.WATCH_STATE_FOR_TRY_NOW, State.PAUSED);
        mModel.set(VideoPlayerProperties.SHOW_SHARE, enableShare());
        mModel.set(VideoPlayerProperties.SHOW_CLOSE, true);
        mModel.set(VideoPlayerProperties.SHOW_PLAY_BUTTON, false);
    }

    @Override
    public void onEnded() {
        VideoTutorialMetrics.recordWatchStateUpdate(mTutorial.featureType, WatchState.COMPLETED);
        mModel.set(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE, areMultipleLanguagesAvailable());
        maybeShowWatchNextVideoButton();
        mModel.set(VideoPlayerProperties.SHOW_TRY_NOW,
                VideoTutorialUtils.shouldShowTryNow(mTutorial.featureType));
        mModel.set(VideoPlayerProperties.WATCH_STATE_FOR_TRY_NOW, State.ENDED);
        mModel.set(VideoPlayerProperties.SHOW_SHARE, enableShare());
        mModel.set(VideoPlayerProperties.SHOW_CLOSE, true);
        mModel.set(VideoPlayerProperties.SHOW_PLAY_BUTTON, true);
    }

    @Override
    public void onError() {
        // TODO(shaktisahu): Determine UI for error state.
    }

    private void changeLanguage() {
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, true);
        mLanguagePicker.showLanguagePicker(
                mTutorial.featureType, this::onLanguageSelected, this::onLanguagePickerClosed);
        VideoTutorialMetrics.recordUserAction(mTutorial.featureType, UserAction.CHANGE_LANGUAGE);
    }

    private void onLanguageSelected() {
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, false);
        mVideoTutorialService.getTutorial(mTutorial.featureType, this::startVideo);
    }

    private void onLanguagePickerClosed() {
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, false);
    }

    private void tryNow() {
        VideoTutorialMetrics.recordUserAction(mTutorial.featureType, UserAction.TRY_NOW);
        mTryNowCallback.onResult(mTutorial);
    }

    private void share() {
        VideoTutorialMetrics.recordUserAction(mTutorial.featureType, UserAction.SHARE);
        VideoTutorialUtils.launchShareIntent(mContext, mTutorial);
    }

    private void close() {
        VideoTutorialMetrics.recordUserAction(mTutorial.featureType, UserAction.CLOSE);
        mCloseCallback.run();
    }

    private void startVideo(Tutorial tutorial) {
        mPlaybackStateObserver.reset();
        VideoTutorialMetrics.recordWatchStateUpdate(mTutorial.featureType, WatchState.STARTED);
        mVideoStartTime = System.currentTimeMillis();
        mTutorial = tutorial;
        LoadUrlParams loadUrlParams =
                new LoadUrlParams(VideoPlayerURLBuilder.buildFromTutorial(tutorial));
        loadUrlParams.setHasUserGesture(true);
        mWebContents.getNavigationController().loadUrl(loadUrlParams);
        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, false);
        hideMediaControls();
    }

    private void maybeShowWatchNextVideoButton() {
        VideoTutorialUtils.getNextTutorial(mVideoTutorialService, mTutorial, nextTutorial -> {
            mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, nextTutorial != null);
        });
    }

    private void onWatchNextClicked() {
        if (mPlaybackStateObserver.getWatchStateInfo().videoWatched()) {
            VideoTutorialMetrics.recordWatchStateUpdate(mTutorial.featureType, WatchState.WATCHED);
        }
        VideoTutorialMetrics.recordUserAction(mTutorial.featureType, UserAction.WATCH_NEXT_VIDEO);
        VideoTutorialUtils.getNextTutorial(mVideoTutorialService, mTutorial, this::startVideo);
    }

    private void hideMediaControls() {
        mModel.set(VideoPlayerProperties.SHOW_TRY_NOW, false);
        mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, false);
        mModel.set(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE, false);
        mModel.set(VideoPlayerProperties.SHOW_SHARE, enableShare());
        mModel.set(VideoPlayerProperties.SHOW_CLOSE, true);
        mModel.set(VideoPlayerProperties.SHOW_PLAY_BUTTON, false);
    }

    private boolean enableShare() {
        return sEnableShareForTesting == null
                ? ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.VIDEO_TUTORIALS, VARIATION_ENABLE_SHARE_BUTTON, true)
                : sEnableShareForTesting;
    }

    private boolean areMultipleLanguagesAvailable() {
        return mVideoTutorialService.getAvailableLanguagesForTutorial(mTutorial.featureType).size()
                > 1;
    }
}
