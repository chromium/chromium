// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.list;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialUtils;
import org.chromium.chrome.browser.video_tutorials.test.TestImageFetcher;
import org.chromium.chrome.browser.video_tutorials.test.TestVideoTutorialService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * Tests for {@link TutorialListCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TutorialListCoordinatorTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Activity mActivity;
    private View mContentView;
    private TestVideoTutorialService mTestVideoTutorialService;
    private TutorialListCoordinator mCoordinator;

    @Mock
    private Callback<Tutorial> mClickCallback;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout parentView = new FrameLayout(mActivity);
            mActivity.setContentView(parentView);
            mTestVideoTutorialService = new TestVideoTutorialService();
            mContentView = LayoutInflater.from(mActivity).inflate(
                    R.layout.video_tutorial_list, null, false);
            parentView.addView(mContentView);

            Bitmap testImage = BitmapFactory.decodeResource(mActivity.getResources(),
                    org.chromium.chrome.browser.video_tutorials.R.drawable.btn_close);
            TestImageFetcher imageFetcher = new TestImageFetcher(testImage);
            mCoordinator = new TutorialListCoordinatorImpl(
                    mContentView.findViewById(R.id.video_tutorial_list), mTestVideoTutorialService,
                    imageFetcher, mClickCallback);
        });
    }

    @Test
    @SmallTest
    public void testShowList() {
        Tutorial tutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        onView(withText(tutorial.title)).check(matches(isDisplayed()));
        onView(withText(VideoTutorialUtils.getVideoLengthString(tutorial.videoLength)))
                .check(matches(isDisplayed()));
        onView(withText(tutorial.title)).perform(ViewActions.click());
        Mockito.verify(mClickCallback).onResult(tutorial);
    }
}
