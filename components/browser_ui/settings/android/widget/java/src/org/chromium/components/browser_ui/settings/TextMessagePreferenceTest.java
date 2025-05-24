// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.app.Activity;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

/** Tests the {@link TextMessagePreference}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TextMessagePreferenceTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Activity mActivity;
    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    static Matcher<View> hasAccessibilityLiveRegion(int liveRegionState) {
        return new TypeSafeMatcher<>() {
            @Override
            protected boolean matchesSafely(View view) {
                return view.getAccessibilityLiveRegion() == liveRegionState;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("View has live region state " + liveRegionState);
            }
        };
    }

    static Matcher<View> hasContentDescription(CharSequence contentDescription) {
        return new TypeSafeMatcher<>() {
            @Override
            protected boolean matchesSafely(View view) {
                return view.getContentDescription() == contentDescription;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("View has content description " + contentDescription);
            }
        };
    }

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceFragment = mSettingsRule.getPreferenceFragment();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
    }

    @Test
    @SmallTest
    public void setAccessibilityLiveRegion() {
        TextMessagePreference preference = new TextMessagePreference(mActivity, null);
        preference.setTitle("Test title");
        preference.setSummary("Test summary");
        preference.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());
        onView(withId(android.R.id.title))
                .check(matches(hasAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE)));
        onView(withId(android.R.id.summary))
                .check(matches(hasAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE)));
    }

    @Test
    @SmallTest
    public void setContentDescription() {
        TextMessagePreference preference = new TextMessagePreference(mActivity, null);
        String title = "Test title";
        String summary = "Test summary";
        String titleCd = "Title content description";
        String summaryCd = "Summary content description";
        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setTitleContentDescription(title);
        preference.setSummaryContentDescription(summary);

        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());
        onView(withId(android.R.id.title)).check(matches(hasContentDescription(title)));
        onView(withId(android.R.id.summary)).check(matches(hasContentDescription(summary)));

        preference.setTitleContentDescription(titleCd);
        onView(withId(android.R.id.title)).check(matches(hasContentDescription(titleCd)));
        onView(withId(android.R.id.summary)).check(matches(hasContentDescription(summary)));

        preference.setSummaryContentDescription(summaryCd);
        onView(withId(android.R.id.title)).check(matches(hasContentDescription(titleCd)));
        onView(withId(android.R.id.summary)).check(matches(hasContentDescription(summaryCd)));
    }
}
