// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import android.app.Activity;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.settings.test.R;

/** Tests of {@link CardWithButtonPreferenceTest}. TODO(crbug.com/376697449): Add a render test. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class CardWithButtonPreferenceTest {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";
    private static final String BUTTON_TEXT = "Button text";

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Activity mActivity;
    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceFragment = mSettingsRule.getPreferenceFragment();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
    }

    @Test
    @SmallTest
    public void testCardWithButtonPreference() {
        CardWithButtonPreference preference = new CardWithButtonPreference(mActivity, null);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setButtonText(BUTTON_TEXT);
        CallbackHelper callback = new CallbackHelper();
        preference.setOnPreferenceClickListener(
                prefClicked -> {
                    callback.notifyCalled();
                    return true;
                });

        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(matches(allOf(withText(SUMMARY), isDisplayed())));
        getButtonView().check(matches(allOf(withText(BUTTON_TEXT), isDisplayed())));

        getButtonView().perform(click());

        Assert.assertEquals(1, callback.getCallCount());
    }

    private ViewInteraction getTitleView() {
        return onView(withId(android.R.id.title));
    }

    private ViewInteraction getSummaryView() {
        return onView(withId(android.R.id.summary));
    }

    private ViewInteraction getButtonView() {
        return onView(withId(R.id.card_button));
    }
}
