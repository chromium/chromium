// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialUtils;
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
    @Mock
    Callback<Tutorial> mTryNowCallback;
    @Mock
    PlaybackStateObserver mPlaybackStateObserver;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);

        Mockito.doReturn(mNavigationController).when(mWebContents).getNavigationController();
        Mockito.doReturn(mResources).when(mContext).getResources();
        mModel = new PropertyModel(VideoPlayerProperties.ALL_KEYS);
        mModel.addObserver(mPropertyObserver);

        VideoPlayerMediator.sEnableShareForTesting = true;
        mTestVideoTutorialService = new TestVideoTutorialService();
        mMediator = new VideoPlayerMediator(mContext, mModel, mTestVideoTutorialService,
                mLanguagePicker, mWebContents, mPlaybackStateObserver, mTryNowCallback,
                mCloseCallback);
    }

    @Test
    public void languagePickerShownFirstTime() {
        mTestVideoTutorialService.setPreferredLocale(null);
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);

        assertThat(mModel.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER), equalTo(true));
        Mockito.verify(mLanguagePicker, Mockito.times(1))
                .showLanguagePicker(
                        eq(tutorial.featureType), mLanguagePickerCallback.capture(), any());
        ((Runnable) mLanguagePickerCallback.getValue()).run();
        Mockito.verify(mNavigationController).loadUrl(any());
    }

    @Test
    public void languagePickerNotShownIfOnlyOneLanguage() {
        mTestVideoTutorialService.setPreferredLocale(null);
        mTestVideoTutorialService.initializeTestLanguages(new String[] {"hi"});
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);

        assertThat(mModel.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER), equalTo(false));
        Mockito.verify(mLanguagePicker, Mockito.times(0))
                .showLanguagePicker(anyInt(), mLanguagePickerCallback.capture(), any());
    }

    @Test
    public void languagePickerNotShownIfPreferredLocaleSetAlready() {
        mTestVideoTutorialService.setPreferredLocale("en");
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);

        assertThat(mModel.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER), equalTo(false));
        Mockito.verify(mLanguagePicker, Mockito.never()).showLanguagePicker(anyInt(), any(), any());
        Mockito.verify(mNavigationController).loadUrl(any());
    }

    @Test
    public void showLoadingScreenDuringStartup() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        Mockito.verify(mNavigationController).loadUrl(any());
        assertThat(mModel.get(VideoPlayerProperties.SHOW_LOADING_SCREEN), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_SHARE), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CLOSE), equalTo(true));

        mMediator.onPlay();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_LOADING_SCREEN), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_SHARE), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CLOSE), equalTo(true));
    }

    @Test
    public void verifyControlsAtPauseState() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        mMediator.onPlay();
        mMediator.onPause();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_SHARE), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CLOSE), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_WATCH_NEXT), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_PLAY_BUTTON), equalTo(false));
    }

    @Test
    public void verifyControlsAtEndState() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        mMediator.onPlay();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_WATCH_NEXT), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE), equalTo(false));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_PLAY_BUTTON), equalTo(false));

        mMediator.onEnded();
        assertThat(mModel.get(VideoPlayerProperties.SHOW_WATCH_NEXT), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE), equalTo(true));
        assertThat(mModel.get(VideoPlayerProperties.SHOW_PLAY_BUTTON), equalTo(true));
    }

    @Test
    public void testChangeLanguage() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        mMediator.onPlay();
        mMediator.onEnded();

        mModel.get(VideoPlayerProperties.CALLBACK_CHANGE_LANGUAGE).run();
        Mockito.verify(mLanguagePicker, Mockito.times(1))
                .showLanguagePicker(
                        eq(tutorial.featureType), mLanguagePickerCallback.capture(), any());
        mTestVideoTutorialService.setPreferredLocale("en");
        ((Runnable) mLanguagePickerCallback.getValue()).run();
    }

    @Test
    public void verifyButtonCallbacks() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        mMediator.onPlay();
        mMediator.onPause();

        mModel.get(VideoPlayerProperties.CALLBACK_TRY_NOW).run();
        Mockito.verify(mTryNowCallback).onResult(tutorial);

        mModel.get(VideoPlayerProperties.CALLBACK_CLOSE).run();
        Mockito.verify(mCloseCallback).run();

        mModel.get(VideoPlayerProperties.CALLBACK_SHARE).run();
        mModel.get(VideoPlayerProperties.CALLBACK_CHANGE_LANGUAGE).run();
        Mockito.verify(mLanguagePicker).showLanguagePicker(eq(tutorial.featureType), any(), any());

        WatchStateInfo watchStateInfo = new WatchStateInfo();
        watchStateInfo.videoLength = 10;
        watchStateInfo.currentPosition = 8;
        Mockito.doReturn(watchStateInfo).when(mPlaybackStateObserver).getWatchStateInfo();
        mModel.get(VideoPlayerProperties.CALLBACK_WATCH_NEXT).run();
        mMediator.destroy();
    }

    @Test
    public void testHandleBackPressed() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        mMediator.playVideoTutorial(tutorial);
        mMediator.onPlay();
        Assert.assertFalse(mMediator.handleBackPressed());
    }

    @Test
    public void testVideoLengthString() {
        assertThat(VideoTutorialUtils.getVideoLengthString(0), equalTo("0:00"));
        assertThat(VideoTutorialUtils.getVideoLengthString(5), equalTo("0:05"));
        assertThat(VideoTutorialUtils.getVideoLengthString(55), equalTo("0:55"));
        assertThat(VideoTutorialUtils.getVideoLengthString(70), equalTo("1:10"));
        assertThat(VideoTutorialUtils.getVideoLengthString(1200), equalTo("20:00"));
        assertThat(VideoTutorialUtils.getVideoLengthString(3615), equalTo("1:00:15"));
    }

    @Test
    public void testTryNowEnabledForFeatures() {
        Assert.assertFalse(VideoTutorialUtils.shouldShowTryNow(FeatureType.CHROME_INTRO));
        Assert.assertFalse(VideoTutorialUtils.shouldShowTryNow(FeatureType.DOWNLOAD));
        Assert.assertTrue(VideoTutorialUtils.shouldShowTryNow(FeatureType.SEARCH));
        Assert.assertTrue(VideoTutorialUtils.shouldShowTryNow(FeatureType.VOICE_SEARCH));
        Assert.assertFalse(VideoTutorialUtils.shouldShowTryNow(99));
    }

    @Test
    public void testVideoPlayerURL() {
        String videoUrl = "https://example/video.mp4";
        String posterUrl = "https://example/poster.png";
        String animationUrl = "https://example/anim.gif";
        String thumbnailUrl = "https://example/thumb.png";
        String captionUrl = "https://example/caption.vtt";
        String shareUrl = "https://example/share.mp4";
        Tutorial testTutorial = new Tutorial(FeatureType.CHROME_INTRO, "title", videoUrl, posterUrl,
                animationUrl, thumbnailUrl, captionUrl, shareUrl, 25);

        assertThat(VideoPlayerURLBuilder.buildFromTutorial(testTutorial),
                equalTo("chrome-untrusted://video-tutorials/"
                        + "?video_url=https://example/video.mp4"
                        + "&poster_url=https://example/poster.png"
                        + "&caption_url=https://example/caption.vtt"));
    }
}
