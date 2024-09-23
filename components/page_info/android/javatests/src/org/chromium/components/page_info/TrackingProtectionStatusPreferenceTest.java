// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.LargeTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;
import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.TrackingProtectionBlockingStatus;
import org.chromium.components.content_settings.TrackingProtectionFeatureType;

import java.util.Arrays;
import java.util.List;

/** Tests for TrackingProtectionStatusPreference. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TrackingProtectionStatusPreferenceTest {
    private static class TestElement {
        public @TrackingProtectionFeatureType int feature;
        public String protectionOn;
        public String protectionOff;
        public @TrackingProtectionBlockingStatus int statusOn;
        public @TrackingProtectionBlockingStatus int statusOff;

        public TestElement(
                @TrackingProtectionFeatureType int feature,
                String protectionOn,
                String protectionOff,
                @TrackingProtectionBlockingStatus int statusOn,
                @TrackingProtectionBlockingStatus int statusOff) {
            this.feature = feature;
            this.protectionOn = protectionOn;
            this.protectionOff = protectionOff;
            this.statusOn = statusOn;
            this.statusOff = statusOff;
        }
    }

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Activity mActivity;
    private PreferenceScreen mPreferenceScreen;
    private List<TestElement> mTestElements;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
        populateTestElements();
    }

    void populateTestElements() {
        mTestElements =
                Arrays.asList(
                        new TestElement(
                                TrackingProtectionFeatureType.THIRD_PARTY_COOKIES,
                                "Third-party cookies limited",
                                "Third-party cookies allowed",
                                TrackingProtectionBlockingStatus.LIMITED,
                                TrackingProtectionBlockingStatus.ALLOWED),
                        new TestElement(
                                TrackingProtectionFeatureType.FINGERPRINTING_PROTECTION,
                                "Digital fingerprinting limited",
                                "Digital fingerprinting allowed",
                                TrackingProtectionBlockingStatus.LIMITED,
                                TrackingProtectionBlockingStatus.ALLOWED),
                        new TestElement(
                                TrackingProtectionFeatureType.IP_PROTECTION,
                                "IP address hidden",
                                "IP address visible",
                                TrackingProtectionBlockingStatus.HIDDEN,
                                TrackingProtectionBlockingStatus.VISIBLE));
    }

    @Test
    @LargeTest
    public void testToggleTrackingProtection() {
        var preference = new TrackingProtectionStatusPreference(mActivity);
        mPreferenceScreen.addPreference(preference);
        // Simulate updates as if Tracking Protection is on.
        for (TestElement element : mTestElements) {
            preference.updateStatus(
                    new TrackingProtectionFeature(
                            element.feature,
                            CookieControlsEnforcement.NO_ENFORCEMENT,
                            element.statusOn),
                    true);
        }

        // Check each element is there and in the correct state.
        for (TestElement element : mTestElements) {
            onView(withText(containsString(element.protectionOn))).check(matches(isDisplayed()));
        }

        // Simulate updates as if Tracking Protection is off.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (TestElement element : mTestElements) {
                        preference.updateStatus(
                                new TrackingProtectionFeature(
                                        element.feature,
                                        CookieControlsEnforcement.NO_ENFORCEMENT,
                                        element.statusOff),
                                true);
                    }
                });
        // Check each element is there and in the protection off state.
        for (TestElement element : mTestElements) {
            onView(withText(containsString(element.protectionOff))).check(matches(isDisplayed()));
        }
    }

    @Test
    @LargeTest
    public void testElementVisibility() {
        var preference = new TrackingProtectionStatusPreference(mActivity);
        mPreferenceScreen.addPreference(preference);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Set all the features to visible.
                    for (TestElement element : mTestElements) {
                        preference.updateStatus(
                                new TrackingProtectionFeature(
                                        element.feature,
                                        CookieControlsEnforcement.NO_ENFORCEMENT,
                                        element.statusOn),
                                true);
                    }
                });

        // Check that hiding each element works.
        for (TestElement element : mTestElements) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        preference.updateStatus(
                                new TrackingProtectionFeature(
                                        element.feature,
                                        CookieControlsEnforcement.NO_ENFORCEMENT,
                                        element.statusOn),
                                false);
                    });
            onView(withText(containsString(element.protectionOn)))
                    .check(matches(not(isDisplayed())));
            // The other elements should still be displayed.
            for (TestElement element2 : mTestElements) {
                if (element.feature != element2.feature) {
                    onView(withText(containsString(element2.protectionOn)))
                            .check(matches(isDisplayed()));
                }
            }
            // Reenable.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        preference.updateStatus(
                                new TrackingProtectionFeature(
                                        element.feature,
                                        CookieControlsEnforcement.NO_ENFORCEMENT,
                                        element.statusOn),
                                true);
                    });
            onView(withText(containsString(element.protectionOn))).check(matches(isDisplayed()));
        }
    }

    @Test
    @LargeTest
    public void testDelayedVisibility() {
        var preference = new TrackingProtectionStatusPreference(mActivity);
        // Set all the features to visible.
        for (TestElement element : mTestElements) {
            preference.updateStatus(
                    new TrackingProtectionFeature(
                            element.feature,
                            CookieControlsEnforcement.NO_ENFORCEMENT,
                            element.statusOn),
                    true);
        }

        // Only add to the PreferenceScreen after configuring.
        mPreferenceScreen.addPreference(preference);

        // The visibility updates should be correctly delayed until onBindViewHolder.
        for (TestElement element : mTestElements) {
            onView(withText(containsString(element.protectionOn))).check(matches(isDisplayed()));
        }
    }

    @IntDef({
        CompoundDrawable.START,
        CompoundDrawable.TOP,
        CompoundDrawable.END,
        CompoundDrawable.BOTTOM
    })
    private @interface CompoundDrawable {
        int START = 0;
        int TOP = 1;
        int END = 2;
        int BOTTOM = 3;
    }

    private static Matcher<View> compoundDrawableVisible(@CompoundDrawable int position) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof TextView)) {
                    return false;
                }
                Drawable[] compoundDrawables = ((TextView) view).getCompoundDrawablesRelative();
                Drawable endDrawable = compoundDrawables[position];
                return endDrawable != null;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with drawable in position " + position);
            }
        };
    }

    @Test
    @LargeTest
    public void testManagedIcons() {
        var preference = new TrackingProtectionStatusPreference(mActivity);
        // Cookies allowed by policy; IPP disabled by setting, FPP enabled.
        preference.updateStatus(
                new TrackingProtectionFeature(
                        TrackingProtectionFeatureType.THIRD_PARTY_COOKIES,
                        CookieControlsEnforcement.ENFORCED_BY_POLICY,
                        TrackingProtectionBlockingStatus.ALLOWED),
                true);
        preference.updateStatus(
                new TrackingProtectionFeature(
                        TrackingProtectionFeatureType.IP_PROTECTION,
                        CookieControlsEnforcement.ENFORCED_BY_COOKIE_SETTING,
                        TrackingProtectionBlockingStatus.VISIBLE),
                true);
        preference.updateStatus(
                new TrackingProtectionFeature(
                        TrackingProtectionFeatureType.FINGERPRINTING_PROTECTION,
                        CookieControlsEnforcement.NO_ENFORCEMENT,
                        TrackingProtectionBlockingStatus.LIMITED),
                true);
        mPreferenceScreen.addPreference(preference);

        // Verify the managed icons.
        onView(withId(R.id.cookie_status))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));
        onView(withId(R.id.ip_status))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));
        onView(withId(R.id.fingerprint_status))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.END))));
    }
}
