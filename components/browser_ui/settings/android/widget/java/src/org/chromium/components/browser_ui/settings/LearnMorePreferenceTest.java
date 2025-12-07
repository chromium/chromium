// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isClickable;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.ViewInteraction;
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

/** Tests the {@link LearnMorePreference}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class LearnMorePreferenceTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Activity mActivity;
    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    static Matcher<View> hasContentDescription(CharSequence contentDescription) {
        return new TypeSafeMatcher<>() {
            @Override
            protected boolean matchesSafely(View view) {
                CharSequence actualContentDescription = view.getContentDescription();
                if (actualContentDescription == null) {
                    return contentDescription == null;
                }
                return actualContentDescription.toString().equals(contentDescription.toString());
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
    public void testLearnMorePreference() {
        LearnMorePreference preference = new LearnMorePreference(mActivity, null);

        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());
        getTitleView().check(matches(withText(R.string.learn_more)));
        getTitleView().check(matches(isClickable()));
    }

    @Test
    @SmallTest
    public void setSettingName() {
        LearnMorePreference preference = new LearnMorePreference(mActivity, null);

        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());

        String settingName = "protected content";
        preference.setLearnMoreSettingName(settingName);
        String contentDescription =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.learn_more_about_setting, settingName);

        getTitleView().check(matches(hasContentDescription(contentDescription)));
    }

    private ViewInteraction getTitleView() {
        return onView(withId(android.R.id.title));
    }
}
