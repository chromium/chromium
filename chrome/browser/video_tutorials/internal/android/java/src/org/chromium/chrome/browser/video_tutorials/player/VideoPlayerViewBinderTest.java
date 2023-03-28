// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo.State;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link VideoPlayerViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class VideoPlayerViewBinderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Activity mActivity;
    private VideoPlayerView mVideoPlayerView;
    private View mLoadingView;
    private View mLanguagePickerView;
    private View mControls;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Mock
    private ThinWebView mThinWebView;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
        mActivity = mActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel = new PropertyModel(VideoPlayerProperties.ALL_KEYS);

            FrameLayout thinWebViewLayout = new FrameLayout(mActivity);
            Mockito.when(mThinWebView.getView()).thenReturn(thinWebViewLayout);

            mVideoPlayerView = new VideoPlayerView(mActivity, mModel, mThinWebView);
            View mainView = mVideoPlayerView.getView();
            mActivity.setContentView(mainView);

            mLanguagePickerView = mainView.findViewById(R.id.language_picker);
            mLoadingView = mainView.findViewById(R.id.loading_root);
            mControls = mainView.findViewById(R.id.player_root);
            mLanguagePickerView.setVisibility(View.GONE);
            mLoadingView.setVisibility(View.GONE);
            mControls.setVisibility(View.GONE);
            mMCP = PropertyModelChangeProcessor.create(
                    mModel, mVideoPlayerView, new VideoPlayerViewBinder());
        });
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMCP.destroy();
            mVideoPlayerView.destroy();
        });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testLoadingAnimation() {
        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, true);
        assertEquals(View.VISIBLE, mLoadingView.getVisibility());
        mModel.set(VideoPlayerProperties.SHOW_LOADING_SCREEN, false);
        assertEquals(View.GONE, mLoadingView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testLanguagePickerVisibility() {
        mModel.set(VideoPlayerProperties.SHOW_LANGUAGE_PICKER, true);
        assertEquals(View.VISIBLE, mLanguagePickerView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testTryNowButton() {
        View tryNowButton = mControls.findViewById(R.id.try_now);
        mModel.set(VideoPlayerProperties.SHOW_TRY_NOW, false);
        assertEquals(View.GONE, tryNowButton.getVisibility());
        mModel.set(VideoPlayerProperties.SHOW_TRY_NOW, true);
        assertEquals(View.VISIBLE, tryNowButton.getVisibility());

        mModel.set(VideoPlayerProperties.WATCH_STATE_FOR_TRY_NOW, State.PAUSED);
        mModel.set(VideoPlayerProperties.WATCH_STATE_FOR_TRY_NOW, State.ENDED);
        AtomicBoolean buttonClicked = new AtomicBoolean();
        mModel.set(VideoPlayerProperties.CALLBACK_TRY_NOW, () -> buttonClicked.set(true));
        tryNowButton.performClick();
        assertTrue(buttonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testWatchNextButton() {
        View watchNextButton = mControls.findViewById(R.id.watch_next);
        mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, false);
        assertEquals(View.GONE, watchNextButton.getVisibility());
        mModel.set(VideoPlayerProperties.SHOW_WATCH_NEXT, true);
        assertEquals(View.VISIBLE, watchNextButton.getVisibility());

        AtomicBoolean buttonClicked = new AtomicBoolean();
        mModel.set(VideoPlayerProperties.CALLBACK_WATCH_NEXT, () -> buttonClicked.set(true));
        watchNextButton.performClick();
        assertTrue(buttonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testPlayButton() {
        View playButton = mControls.findViewById(R.id.play_button);
        mModel.set(VideoPlayerProperties.SHOW_PLAY_BUTTON, false);
        assertEquals(View.GONE, playButton.getVisibility());
        mModel.set(VideoPlayerProperties.SHOW_PLAY_BUTTON, true);
        assertEquals(View.VISIBLE, playButton.getVisibility());

        AtomicBoolean buttonClicked = new AtomicBoolean();
        mModel.set(VideoPlayerProperties.CALLBACK_PLAY_BUTTON, () -> buttonClicked.set(true));
        playButton.performClick();
        assertTrue(buttonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testChangeLanguageButton() {
        TextView changeLanguage = mControls.findViewById(R.id.change_language);
        String languageName = "XYZ";
        mModel.set(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE, false);
        assertEquals(View.GONE, changeLanguage.getVisibility());
        mModel.set(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE, true);
        mModel.set(VideoPlayerProperties.CHANGE_LANGUAGE_BUTTON_TEXT, languageName);
        assertEquals(View.VISIBLE, changeLanguage.getVisibility());
        assertTrue(TextUtils.equals(languageName, changeLanguage.getText()));

        AtomicBoolean buttonClicked = new AtomicBoolean();
        mModel.set(VideoPlayerProperties.CALLBACK_CHANGE_LANGUAGE, () -> buttonClicked.set(true));
        changeLanguage.performClick();
        assertTrue(buttonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testShareButton() {
        View shareButton = mControls.findViewById(R.id.share_button);
        mModel.set(VideoPlayerProperties.SHOW_SHARE, true);
        assertEquals(View.VISIBLE, shareButton.getVisibility());
        AtomicBoolean buttonClicked = new AtomicBoolean();
        mModel.set(VideoPlayerProperties.CALLBACK_SHARE, () -> buttonClicked.set(true));
        shareButton.performClick();
        assertTrue(buttonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testCloseButton() {
        View closeButton = mControls.findViewById(R.id.close_button);
        AtomicBoolean buttonClicked = new AtomicBoolean();
        mModel.set(VideoPlayerProperties.CALLBACK_CLOSE, () -> buttonClicked.set(true));
        closeButton.performClick();
        assertTrue(buttonClicked.get());
    }
}
