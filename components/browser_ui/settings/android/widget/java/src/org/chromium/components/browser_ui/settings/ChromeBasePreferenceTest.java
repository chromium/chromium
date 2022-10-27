// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.stringContainsInOrder;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableList;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.Arrays;
import java.util.List;

/**
 * Tests of {@link ChromeBasePreference}.
 */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeBasePreferenceTest {
    @ClassRule
    public static final DisableAnimationsTestRule disableAnimationsRule =
            new DisableAnimationsTestRule();
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";

    private Activity mActivity;
    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    private boolean mEnableHighlightManagedPrefDisclaimerAndroid;

    public ChromeBasePreferenceTest(boolean enableHighlightManagedPrefDisclaimerAndroid) {
        mEnableHighlightManagedPrefDisclaimerAndroid = enableHighlightManagedPrefDisclaimerAndroid;
    }

    @Before
    public void setUp() {
        FeatureList.TestValues testValuesOverride = new FeatureList.TestValues();
        testValuesOverride.addFeatureFlagOverride(
                SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID,
                mEnableHighlightManagedPrefDisclaimerAndroid);
        FeatureList.setTestValues(testValuesOverride);

        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceFragment = mSettingsRule.getPreferenceFragment();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
    }

    @Test
    @SmallTest
    public void testUnmanagedPreference() {
        ChromeBasePreference preference = new ChromeBasePreference(mActivity);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setManagedPreferenceDelegate(ManagedPreferencesUtilsTest.UNMANAGED_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(matches(allOf(withText(SUMMARY), isDisplayed())));
        getIconView().check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testPolicyManagedPreferenceWithoutSummary() {
        ChromeBasePreference preference = new ChromeBasePreference(mActivity);
        preference.setTitle(TITLE);
        preference.setManagedPreferenceDelegate(ManagedPreferencesUtilsTest.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        if (mEnableHighlightManagedPrefDisclaimerAndroid) {
            getManagedDisclaimerView().check(
                    matches(allOf(withText(R.string.managed_by_your_organization),
                            hasDrawableStart(), isDisplayed())));
            getIconView().check(matches(not(isDisplayed())));
        } else {
            getSummaryView().check(
                    matches(allOf(withText(R.string.managed_by_your_organization), isDisplayed())));
            getIconView().check(matches(isDisplayed()));
        }
    }

    @Test
    @SmallTest
    public void testPolicyManagedPreferenceWithSummary() {
        ChromeBasePreference preference = new ChromeBasePreference(mActivity);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setManagedPreferenceDelegate(ManagedPreferencesUtilsTest.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        if (mEnableHighlightManagedPrefDisclaimerAndroid) {
            getSummaryView().check(matches(allOf(withText(SUMMARY), isDisplayed())));
            getManagedDisclaimerView().check(
                    matches(allOf(withText(R.string.managed_by_your_organization),
                            hasDrawableStart(), isDisplayed())));
            getIconView().check(matches(not(isDisplayed())));
        } else {
            List<String> expectedSummaryContains = ImmutableList.of(
                    SUMMARY, mActivity.getString(R.string.managed_by_your_organization));
            getSummaryView().check(matches(allOf(
                    withText(stringContainsInOrder(expectedSummaryContains)), isDisplayed())));
            getIconView().check(matches(isDisplayed()));
        }
    }

    @Test
    @SmallTest
    public void testSingleCustodianManagedPreference() {
        ChromeBasePreference preference = new ChromeBasePreference(mActivity);
        preference.setTitle(TITLE);
        preference.setManagedPreferenceDelegate(
                ManagedPreferencesUtilsTest.SINGLE_CUSTODIAN_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(
                matches(allOf(withText(R.string.managed_by_your_parent), isDisplayed())));
        getIconView().check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testMultipleCustodianManagedPreference() {
        ChromeBasePreference preference = new ChromeBasePreference(mActivity);
        preference.setTitle(TITLE);
        preference.setManagedPreferenceDelegate(
                ManagedPreferencesUtilsTest.MULTI_CUSTODIAN_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(
                matches(allOf(withText(R.string.managed_by_your_parents), isDisplayed())));
        getIconView().check(matches(isDisplayed()));
    }

    private ViewInteraction getTitleView() {
        return onView(withId(android.R.id.title));
    }

    private ViewInteraction getSummaryView() {
        return onView(withId(android.R.id.summary));
    }

    private ViewInteraction getManagedDisclaimerView() {
        return onView(withId(R.id.managed_disclaimer_text));
    }

    private ViewInteraction getIconView() {
        return onView(withId(android.R.id.icon));
    }

    private static Matcher<View> hasDrawableStart() {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View view) {
                return ((TextView) view).getCompoundDrawablesRelative()[0] != null;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Expected TextView to define android:drawableStart");
            }
        };
    }

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = Arrays.asList(
            new ParameterSet().value(true).name("EnableHighlightManagedPrefDisclaimerAndroid"),
            new ParameterSet().value(false).name("DisableHighlightManagedPrefDisclaimerAndroid"));
}
