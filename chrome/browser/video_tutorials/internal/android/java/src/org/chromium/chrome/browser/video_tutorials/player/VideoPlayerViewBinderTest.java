// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.support.test.rule.ActivityTestRule;
import android.text.TextUtils;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link VideoPlayerViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class VideoPlayerViewBinderTest {
    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    private Activity mActivity;
    private View mMainView;
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
        mModel = new PropertyModel(VideoPlayerProperties.ALL_KEYS);
        mActivity = mActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout thinWebViewLayout = new FrameLayout(mActivity);
            Mockito.when(mThinWebView.getView()).thenReturn(thinWebViewLayout);

            VideoPlayerView videoPlayerView = new VideoPlayerView(mActivity, mModel, mThinWebView);
            mMainView = videoPlayerView.getView();
            mActivity.setContentView(mMainView);

            mLanguagePickerView = mMainView.findViewById(R.id.language_picker);
            mLoadingView = mMainView.findViewById(R.id.loading_root);
            mControls = mMainView.findViewById(R.id.player_root);
            mLanguagePickerView.setVisibility(View.GONE);
            mLoadingView.setVisibility(View.GONE);
            mControls.setVisibility(View.GONE);
            mMCP = PropertyModelChangeProcessor.create(
                    mModel, videoPlayerView, new VideoPlayerViewBinder());
        });
    }

    @After
    public void tearDown() throws Exception {
        mMCP.destroy();
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
    public void testControlsVisibility() {
        mModel.set(VideoPlayerProperties.SHOW_MEDIA_CONTROLS, true);
        assertEquals(View.VISIBLE, mControls.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testTryNowButton() {
        View tryNowButton = mControls.findViewById(R.id.try_now);
        mModel.set(VideoPlayerProperties.SHOW_TRY_NOW, true);
        assertEquals(View.VISIBLE, tryNowButton.getVisibility());

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
    public void testChangeLanguageButton() {
        TextView changeLanguage = mControls.findViewById(R.id.change_language);
        String languageName = "XYZ";
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
