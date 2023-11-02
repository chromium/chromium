// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.SnoozeAction;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Integration tests for UserEducationHelper. */
@RunWith(BaseJUnit4ClassRunner.class)
public final class UserEducationHelperTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    private Tracker mTracker;

    @Mock
    private Profile mProfile;

    private static Activity sActivity;
    private FrameLayout mContentView;
    private TestValues mTestValues = new TestValues();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Pretend the feature engagement feature is already initialized. Otherwise
        // UserEducationHelper#requestShowIPH() calls get dropped during test.
        doAnswer(invocation -> {
            invocation.<Callback<Boolean>>getArgument(0).onResult(true);
            return null;
        })
                .when(mTracker)
                .addOnInitializedCallback(any());
        TrackerFactory.setTrackerForTests(mTracker);

        // When snoozing is enabled, do not show any unwanted IPH bubbles.
        when(mTracker.shouldTriggerHelpUIWithSnooze(any()))
                .thenReturn(new TriggerDetails(false, false));

        activityTestRule.launchActivity(null);
        Profile.setLastUsedProfileForTesting(mProfile);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivity = activityTestRule.getActivity();
            mContentView = new FrameLayout(sActivity);
            ;
            sActivity.setContentView(mContentView);
            Button button = new Button(sActivity);
            button.setLayoutParams(new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT));
            button.setText("Dummy");
            button.setTag("Dummy");
            mContentView.addView(button);
        });
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
        FeatureList.setTestValues(null);
    }

    @Test
    @MediumTest
    public void testShowIPHWithSnooze() throws Throwable {
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.SNOOZABLE_IPH, true);
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.ENABLE_AUTOMATIC_SNOOZE, false);
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.ENABLE_IPH, true);
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS, false);
        FeatureList.setTestValues(mTestValues);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mTracker.shouldTriggerHelpUIWithSnooze(FeatureConstants.DOWNLOAD_HOME_FEATURE))
                    .thenReturn(new TriggerDetails(true, true));
            UserEducationHelper userEducationHelper =
                    new UserEducationHelper(sActivity, new Handler());
            View homeButton = mContentView.findViewWithTag("Dummy");
            userEducationHelper.requestShowIPH(
                    new IPHCommandBuilder(sActivity.getResources(),
                            FeatureConstants.DOWNLOAD_HOME_FEATURE, R.id.promo_description,
                            R.id.promo_description)
                            .setAnchorView(homeButton)
                            .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                            .build());
        });
        onView(withId(R.id.button_snooze))
                .inRoot(withDecorView(not(is(sActivity.getWindow().getDecorView()))))
                .perform(ViewActions.click());

        verify(mTracker, times(1))
                .dismissedWithSnooze(FeatureConstants.DOWNLOAD_HOME_FEATURE, SnoozeAction.SNOOZED);
    }
}