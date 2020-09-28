// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import android.content.Context;

import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialUtils;
import org.chromium.chrome.browser.video_tutorials.languages.LanguagePickerCoordinator;
import org.chromium.chrome.browser.video_tutorials.languages.LanguageUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator for the video player UI, responsible for changing the state of UI based on user
 * interaction events, and player state.
 */
class VideoPlayerMediator implements PlaybackStateObserver.Observer {
    private final Context mContext;
    private final VideoTutorialService mVideoTutorialService;
    private final PropertyModel mModel;
    private final LanguagePickerCoordinator mLanguagePicker;
    private final WebContents mWebContents;
    private Tutorial mTutorial;
    private final Runnable mCloseCallback;

    /** Constructor. */
    public VideoPlayerMediator(Context context, PropertyModel model,
            VideoTutorialService videoTutorialService, LanguagePickerCoordinator languagePicker,
            WebContents webContents, Runnable closeCallback) {
        mContext = context;
        mModel = model;
        mVideoTutorialService = videoTutorialService;
        mLanguagePicker = languagePicker;
        mWebContents = webContents;
        mCloseCallback = closeCallback;

        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, false);
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, false);
        mModel.set(VideoPlayerProperties.SHOW_MEDIA_CONTROLS, false);
        mModel.set(VideoPlayerProperties.CALLBACK_WATCH_NEXT, this::onWatchNextClicked);
        mModel.set(VideoPlayerProperties.CALLBACK_CHANGE_LANGUAGE, this::changeLanguage);
        mModel.set(VideoPlayerProperties.CALLBACK_TRY_NOW, this::tryNow);
        mModel.set(VideoPlayerProperties.CALLBACK_SHARE, this::share);
        mModel.set(VideoPlayerProperties.CALLBACK_CLOSE, closeCallback);
    }

    /**
     * Entry point for playing a tutorial video. Shows the language picker if it is the very first
     * time.
     */
    void playVideoTutorial(Tutorial tutorial) {
        mTutorial = tutorial;

        if (mVideoTutorialService.getPreferredLocale() == null) {
            mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, true);
            mLanguagePicker.showLanguagePicker(this::onLanguageSelected, mCloseCallback);
        } else {
            startVideo(tutorial);
        }
    }

    @Override
    public void onPlay() {
        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, false);
        mModel.set(VideoPlayerProperties.SHOW_MEDIA_CONTROLS, false);
    }

    @Override
    public void onPause() {
        mModel.set(VideoPlayerProperties.SHOW_MEDIA_CONTROLS, true);
        mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, false);
        mModel.set(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE, false);
    }

    @Override
    public void onEnded() {
        mModel.set(VideoPlayerProperties.SHOW_MEDIA_CONTROLS, true);
        mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, true);
        mModel.set(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE, true);
        updateChangeLanguageButtonText();

        VideoTutorialUtils.getNextTutorial(mVideoTutorialService, mTutorial, nextTutorial -> {
            mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, nextTutorial != null);
        });
    }

    private void changeLanguage() {
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, true);
        mLanguagePicker.showLanguagePicker(this::onLanguageSelected, () -> {} /* closeCallback */);
    }

    private void updateChangeLanguageButtonText() {
        String language = LanguageUtils.getLanguageForLocale(
                mContext.getResources(), mVideoTutorialService.getPreferredLocale());
        String buttonText = mContext.getResources().getString(
                R.string.video_tutorials_change_language, language == null ? "" : language);
        mModel.set(VideoPlayerProperties.CHANGE_LANGUAGE_BUTTON_TEXT, buttonText);
    }

    private void onLanguageSelected() {
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, false);
        updateChangeLanguageButtonText();
        mVideoTutorialService.getTutorial(mTutorial.featureType, this::startVideo);
    }

    private void tryNow() {}

    private void share() {}

    private void startVideo(Tutorial tutorial) {
        LoadUrlParams loadUrlParams =
                new LoadUrlParams(VideoPlayerURLBuilder.buildFromTutorial(tutorial));
        loadUrlParams.setHasUserGesture(true);
        mWebContents.getNavigationController().loadUrl(loadUrlParams);
        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, false);
        mModel.set(VideoPlayerProperties.SHOW_MEDIA_CONTROLS, false);
    }

    private void onWatchNextClicked() {
        VideoTutorialUtils.getNextTutorial(mVideoTutorialService, mTutorial, this::startVideo);
    }
}
