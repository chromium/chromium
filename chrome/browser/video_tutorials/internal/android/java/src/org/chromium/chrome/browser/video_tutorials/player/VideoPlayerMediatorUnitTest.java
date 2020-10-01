// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.ArgumentMatchers.any;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.languages.LanguagePickerCoordinator;
import org.chromium.chrome.browser.video_tutorials.test.TestVideoTutorialService;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

/**
 * Tests for {@link VideoPlayerMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class VideoPlayerMediatorUnitTest {
    private TestVideoTutorialService mTestVideoTutorialService;
    private PropertyModel mModel;
    private VideoPlayerMediator mMediator;
    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    private LanguagePickerCoordinator mLanguagePicker;
    @Mock
    WebContents mWebContents;
    @Mock
    NavigationController mNavigationController;
    @Mock
    Runnable mCloseCallback;

    @Captor
    ArgumentCaptor<Runnable> mLanguagePickerCallback;
    @Mock
    PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);

        Mockito.doReturn(mNavigationController).when(mWebContents).getNavigationController();
        Mockito.doReturn(mResources).when(mContext).getResources();
        mModel = new PropertyModel(VideoPlayerProperties.ALL_KEYS);
        mModel.addObserver(mPropertyObserver);

        mTestVideoTutorialService = new TestVideoTutorialService();
        mMediator = new VideoPlayerMediator(mContext, mModel, mTestVideoTutorialService,
                mLanguagePicker, mWebContents, mCloseCallback);
    }

    @Test
    public void languagePickerShownFirstTime() {
        mTestVideoTutorialService.setPreferredLocale(null);
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);

        assertThat(mModel.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER), equalTo(true));
        Mockito.verify(mLanguagePicker, Mockito.times(1))
                .showLanguagePicker(mLanguagePickerCallback.capture(), any());
        ((Runnable) mLanguagePickerCallback.getValue()).run();
        Mockito.verify(mNavigationController).loadUrl(any());
    }

    @Test
    public void languagePickerNotShownIfPreferredLocaleSetAlready() {
        mTestVideoTutorialService.setPreferredLocale(TestVideoTutorialService.ENGLISH.locale);
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);

        assertThat(mModel.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER), equalTo(false));
        Mockito.verify(mLanguagePicker, Mockito.never()).showLanguagePicker(any(), any());
        Mockito.verify(mNavigationController).loadUrl(any());
    }

    @Test
    public void showLoadingScreenDuringStartup() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        Mockito.verify(mNavigationController).loadUrl(any());
        assertThat(mModel.get(VideoPlayerProperties.SHOW_LOADING_SCREEN), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_MEDIA_CONTROLS), equalTo(false));

        mMediator.onPlay();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_LOADING_SCREEN), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_MEDIA_CONTROLS), equalTo(false));
    }

    @Test
    public void verifyControlsAtPauseState() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        mMediator.onPlay();
        mMediator.onPause();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_MEDIA_CONTROLS), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_WATCH_NEXT), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE), equalTo(false));
    }

    @Test
    public void verifyControlsAtEndState() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        mMediator.onPlay();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_WATCH_NEXT), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE), equalTo(false));

        mMediator.onEnded();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_WATCH_NEXT), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE), equalTo(true));
    }
}
