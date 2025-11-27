// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.os.Build;
import android.view.View;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.components.browser_ui.settings.test.R;

/** Tests of {@link ChromeButtonPreference}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeButtonPreferenceTest {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";
    private static final int TEXT_RES = R.string.ok;
    private static final int CONTENT_DESCRIPTION_RES = R.string.ok;

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
    public void testChromeButtonPreference() {
        ChromeButtonPreference preference = new ChromeButtonPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setButton(TEXT_RES, CONTENT_DESCRIPTION_RES, null);
        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(matches(allOf(withText(SUMMARY), isDisplayed())));
        getButtonWidget().check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testChromeButtonPreferenceManaged() {
        ChromeButtonPreference preference = new ChromeButtonPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setButton(TEXT_RES, CONTENT_DESCRIPTION_RES, null);
        preference.setManagedPreferenceDelegate(ManagedPreferenceTestDelegates.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView()
                .check(
                        matches(
                                allOf(
                                        withText(R.string.managed_by_your_organization),
                                        isDisplayed())));
        getButtonWidget().check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.VANILLA_ICE_CREAM,
            message = "crbug.com/464320061")
    public void testSetButtonWithStringsAndListener() {
        ChromeButtonPreference preference = new ChromeButtonPreference(mActivity);
        String buttonText = "Custom Text";
        String contentDescription = "Custom Description";

        final boolean[] clicked = {false};
        View.OnClickListener listener = v -> clicked[0] = true;

        preference.setButton(buttonText, contentDescription, listener);
        mPreferenceScreen.addPreference(preference);

        onView(withText(buttonText)).check(matches(isDisplayed()));
        onView(withText(buttonText)).perform(click());
        Assert.assertTrue("Button listener should have been triggered", clicked[0]);
    }

    @Test
    @SmallTest
    public void testButtonEnabled() {
        ChromeButtonPreference preference = new ChromeButtonPreference(mActivity);
        preference.setButton(TEXT_RES, CONTENT_DESCRIPTION_RES, null);
        preference.setButtonEnabled(false);
        mPreferenceScreen.addPreference(preference);

        getButtonWidget().check(matches(not(isEnabled())));

        // Verify re-enabling works.
        // Note: In a real app, we might need to run this on the UI thread if the view is already
        // attached.
        // For unit tests with Espresso, modifying the view state from the test thread might be
        // flaky if
        // the view hierarchy is actively being drawn, but for this setup it's often okay.
        // However, to be safe and correct:
        org.chromium.base.ThreadUtils.runOnUiThreadBlocking(
                () -> preference.setButtonEnabled(true));
        getButtonWidget().check(matches(isEnabled()));
    }

    @Test
    @SmallTest
    public void testBackgroundColor() {
        ChromeButtonPreference preference = new ChromeButtonPreference(mActivity);
        preference.setButton(TEXT_RES, CONTENT_DESCRIPTION_RES, null);
        // Verify it doesn't crash
        preference.setBackgroundColor(android.R.color.black);
        mPreferenceScreen.addPreference(preference);
    }

    @Test
    @SmallTest
    public void testIsManaged() {
        ChromeButtonPreference preference = new ChromeButtonPreference(mActivity);
        Assert.assertFalse(preference.isManaged());

        preference.setManagedPreferenceDelegate(ManagedPreferenceTestDelegates.POLICY_DELEGATE);
        Assert.assertTrue(preference.isManaged());
    }

    private ViewInteraction getTitleView() {
        return onView(withId(android.R.id.title));
    }

    private ViewInteraction getSummaryView() {
        return onView(withId(android.R.id.summary));
    }

    private ViewInteraction getButtonWidget() {
        return onView(withId(R.id.button_widget));
    }
}
