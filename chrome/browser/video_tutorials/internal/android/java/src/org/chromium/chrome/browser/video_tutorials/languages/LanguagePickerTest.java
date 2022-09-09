// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.LanguageInfoProvider;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.chrome.browser.video_tutorials.test.TestVideoTutorialService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * Tests for {@link LanguagePickerCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class LanguagePickerTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Activity mActivity;
    private View mContentView;
    private VideoTutorialService mVideoTutorialService;
    private LanguagePickerCoordinator mCoordinator;
    @Mock
    private LanguageInfoProvider mLanguageProvider;

    @Mock
    private Runnable mWatchCallback;
    @Mock
    private Runnable mCloseCallback;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout parentView = new FrameLayout(mActivity);
            mActivity.setContentView(parentView);
            mVideoTutorialService = new TestVideoTutorialService();
            mContentView =
                    LayoutInflater.from(mActivity).inflate(R.layout.language_picker, null, false);
            parentView.addView(mContentView);
            Mockito.when(mLanguageProvider.getLanguageInfo("hi"))
                    .thenReturn(TestVideoTutorialService.HINDI);
            mCoordinator = new LanguagePickerCoordinator(
                    mContentView, mVideoTutorialService, mLanguageProvider);
            mCoordinator.showLanguagePicker(
                    FeatureType.CHROME_INTRO, mWatchCallback, mCloseCallback);
        });
    }

    @Test
    @SmallTest
    public void testShowLanguages() {
        onView(withText(TestVideoTutorialService.HINDI.name)).check(matches(isDisplayed()));
        onView(withText(TestVideoTutorialService.HINDI.nativeName)).check(matches(isDisplayed()));
        onView(withText("Watch")).check(matches(isDisplayed())).perform(ViewActions.click());
        Mockito.verify(mWatchCallback).run();
    }
}
