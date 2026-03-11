// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.view.ViewStub;

import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.test.R;

/**
 * Tests for {@link ChromeExpandableSwitchPreference}. This class verifies the behavior of the
 * expandable switch preference, including: 1. Initial collapsed state. 2. Expansion and collapse
 * logic. 3. Lazy inflation of the expanded content.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeExpandableSwitchPreferenceTest {
    // Step 1: Set up the Test Rule
    // This rule launches a blank activity that can host our settings fragment.
    // Ideally, we'd mock everything, but for UI widget tests, running in a real activity
    // with Espresso is the standard way to verify view hierarchy and interactions.
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private static final String PREF_KEY = "expandable_switch";
    private static final String TITLE = "Expandable Switch";
    private static final String SUMMARY = "Summary";

    private Activity mActivity;
    private ChromeExpandableSwitchPreference mPreference;

    // Step 2: Initialization
    // The @Before method runs before every single @Test method.
    // Here we inflate the XML resource we created into the test activity.
    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        PreferenceFragmentCompat fragment = mSettingsRule.getPreferenceFragment();

        // Inflate our test preference screen.
        // This is crucial because our preference requires XML attributes (like
        // expandedContentLayout)
        // to be valid due to the assertion we added in the constructor.
        SettingsUtils.addPreferencesFromResource(
                fragment, R.xml.test_chrome_expandable_switch_preference_screen);

        mPreference = (ChromeExpandableSwitchPreference) fragment.findPreference(PREF_KEY);
    }

    // Step 3: Test Cases

    @Test
    @LargeTest
    public void testCollapsedState() {
        // Verify the initial state of the preference.
        // It should be visible, enabled, and collapsed by default.

        // Check title and summary are displayed
        onView(withText(TITLE)).check(matches(isDisplayed()));
        onView(withText(SUMMARY)).check(matches(isDisplayed()));

        // Check the expand button (the arrow) is present but NOT checked (meaning collapsed).
        // R.id.expandable_switch_expand_icon is the ID of the expand arrow in the layout.
        onView(withId(R.id.expandable_switch_expand_icon))
                .check(matches(allOf(isDisplayed(), not(isChecked()))));

        // Check that the expanded area is GONE (not displayed).
        onView(withId(R.id.expandable_switch_expanded_area_stub))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @LargeTest
    public void testExpandedState() {
        // Step 4: Interaction
        // Expand programmatically to ensure the logic works.
        ThreadUtils.runOnUiThreadBlocking(() -> mPreference.setExpanded(true));

        // The button should now be checked (arrow pointing up usually, or just checked state).
        onView(withId(R.id.expandable_switch_expand_icon)).check(matches(isChecked()));

        // The expanded area should now be visible.
        onView(withId(R.id.expandable_switch_expanded_area)).check(matches(isDisplayed()));

        // Verify the content we specified in the XML (chrome_managed_preference_with_custom_layout)
        // was actually inflated. That layout contains an icon frame with id @android:id/icon_frame.
        // We check if that view exists and is displayed.
        onView(withId(android.R.id.icon_frame)).check(matches(isDisplayed()));

        // Step 5: Collapse programmatically
        ThreadUtils.runOnUiThreadBlocking(() -> mPreference.setExpanded(false));

        // Verify it went back to collapsed state.
        onView(withId(R.id.expandable_switch_expand_icon)).check(matches(not(isChecked())));
        // Note: once inflated, the view stays in the hierarchy but becomes GONE.
        onView(withId(R.id.expandable_switch_expanded_area)).check(matches(not(isDisplayed())));
    }

    @Test
    @LargeTest
    public void testLazyInflation() {
        // This test verifies our "micro-optimization".
        // We want to ensure the view is NOT inflated until we expand.

        // Verify initial state: expanded_area_stub exists but expanded_area does not.
        onView(withId(R.id.expandable_switch_expanded_area_stub))
                .check(
                        (view, noViewFoundException) -> {
                            if (noViewFoundException != null) {
                                throw noViewFoundException;
                            }
                            if (!(view instanceof ViewStub)) {
                                throw new AssertionError("View is not a ViewStub");
                            }
                        });

        // Expand programmatically
        ThreadUtils.runOnUiThreadBlocking(() -> mPreference.setExpanded(true));

        // Verify after expansion: expanded_area should now exist and be visible.
        onView(withId(R.id.expandable_switch_expanded_area)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testClickOnExpandedArea() {
        // Expand programmatically.
        ThreadUtils.runOnUiThreadBlocking(() -> mPreference.setExpanded(true));

        // Verify that the preference is expanded.
        onView(withId(R.id.expandable_switch_expanded_area)).check(matches(isDisplayed()));

        // Click on the expanded area.
        onView(withId(R.id.expandable_switch_expanded_area)).perform(click());

        // Verify that the preference is still expanded.
        onView(withId(R.id.expandable_switch_expanded_area)).check(matches(isDisplayed()));
        onView(withId(R.id.expandable_switch_expand_icon)).check(matches(isChecked()));
    }
}
