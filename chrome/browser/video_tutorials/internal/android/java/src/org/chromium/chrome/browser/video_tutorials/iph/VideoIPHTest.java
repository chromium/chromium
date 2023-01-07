// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.ViewStub;
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
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.test.TestImageFetcher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.HashMap;

/**
 * Tests for {@link LanguagePickerCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class VideoIPHTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Activity mActivity;
    private VideoIPHCoordinator mCoordinator;

    @Mock
    private Callback<Tutorial> mOnClickListener;
    @Mock
    private Callback<Tutorial> mOnDismissListener;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout parentView = new FrameLayout(mActivity);
            mActivity.setContentView(parentView);
            ViewStub viewStub = new ViewStub(mActivity);
            viewStub.setLayoutResource(R.layout.video_tutorial_iph_card);
            parentView.addView(viewStub);

            Bitmap testImage = BitmapFactory.decodeResource(mActivity.getResources(),
                    org.chromium.chrome.browser.video_tutorials.R.drawable.btn_close);
            TestImageFetcher imageFetcher = new TestImageFetcher(testImage);
            mCoordinator = new VideoIPHCoordinatorImpl(
                    viewStub, imageFetcher, mOnClickListener, mOnDismissListener);
            FeatureList.setTestFeatures(new HashMap<String, Boolean>());
        });
    }

    @Test
    @SmallTest
    public void testShowIPH() {
        final Tutorial tutorial = createDummyTutorial();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mCoordinator.showVideoIPH(tutorial); });
        onView(withText(tutorial.title)).check(matches(isDisplayed()));
        onView(withText("5:35")).check(matches(isDisplayed()));
        onView(withText(tutorial.title)).perform(ViewActions.click());
        Mockito.verify(mOnClickListener).onResult(Mockito.any());
        onView(withId(R.id.close_button)).perform(ViewActions.click());
        Mockito.verify(mOnDismissListener).onResult(Mockito.any());
    }

    private Tutorial createDummyTutorial() {
        return new Tutorial(FeatureType.DOWNLOAD,
                "How to use Google Chrome's download functionality",
                "https://xyz.example.com/xyz.mp4", "https://xyz.example.com/xyz.png",
                "https://xyz.example.com/xyz.gif", "https://xyz.example.com/xyz.png",
                "https://xyz.example.com/xyz.vtt", "https://xyz.example.com/xyz.mp4", 335);
    }

}
