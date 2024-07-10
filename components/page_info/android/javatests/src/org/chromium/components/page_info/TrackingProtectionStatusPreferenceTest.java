// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.Matchers.not;

import android.app.Activity;

import androidx.preference.PreferenceScreen;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;
import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.TrackingProtectionBlockingStatus;
import org.chromium.components.content_settings.TrackingProtectionFeatureType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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
        // 3PCD are limited and not completely blocked.
        preference.setBlockAll3PC(false);
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
        TestThreadUtils.runOnUiThreadBlocking(
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // 3PCD are limited and not completely blocked.
                    preference.setBlockAll3PC(false);
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
            TestThreadUtils.runOnUiThreadBlocking(
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
            TestThreadUtils.runOnUiThreadBlocking(
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
        // 3PCD are limited and not completely blocked.
        preference.setBlockAll3PC(false);
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
}
