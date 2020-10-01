// Copyright 2020 The Chromium Authors. All rights reserved.
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
import android.support.test.rule.ActivityTestRule;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
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
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivity;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Tests for {@link LanguagePickerCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class VideoIPHTest {
    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    private Activity mActivity;
    private VideoIPHCoordinator mCoordinator;

    @Mock
    private Callback<Tutorial> mOnClickListener;
    @Mock
    private Callback<Tutorial> mOnDismissListener;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
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
        });
    }

    @Test
    @SmallTest
    public void testShowIPH() {
        final Tutorial tutorial = createDummyTutorial();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mCoordinator.showVideoIPH(tutorial); });
        onView(withText(tutorial.displayTitle)).check(matches(isDisplayed()));
        onView(withText("5:35")).check(matches(isDisplayed()));
        onView(withText(tutorial.displayTitle)).perform(ViewActions.click());
        Mockito.verify(mOnClickListener).onResult(Mockito.any());
        onView(withId(R.id.close_button)).perform(ViewActions.click());
        Mockito.verify(mOnDismissListener).onResult(Mockito.any());
    }

    private Tutorial createDummyTutorial() {
        return new Tutorial(FeatureType.DOWNLOAD,
                "How to use Google Chrome's download functionality",
                "https://xyz.example.com/xyz.mp4", "https://xyz.example.com/xyz.png",
                "https://xyz.example.com/xyz.vtt", "https://xyz.example.com/xyz.mp4", 335);
    }

    private static class TestImageFetcher extends ImageFetcher.ImageFetcherForTesting {
        private final Bitmap mBitmapToFetch;

        TestImageFetcher(@Nullable Bitmap bitmapToFetch) {
            mBitmapToFetch = bitmapToFetch;
        }

        @Override
        public void fetchGif(final ImageFetcher.Params params, Callback<BaseGifImage> callback) {}

        @Override
        public void fetchImage(ImageFetcher.Params params, Callback<Bitmap> callback) {
            callback.onResult(mBitmapToFetch);
        }

        @Override
        public void clear() {}

        @Override
        public @ImageFetcherConfig int getConfig() {
            return ImageFetcherConfig.IN_MEMORY_ONLY;
        }

        @Override
        public void destroy() {}
    }
}
