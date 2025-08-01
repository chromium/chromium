// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.preference.PreferenceViewHolder.createInstanceForTests;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ToastManager;

/** Tests of {@link ManagedPreferencesUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class ManagedPreferencesUtilsTest {
    private Activity mActivity;
    private Preference mPreference;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mPreference = new Preference(mActivity);
    }

    @After
    public void tearDown() {
        ShadowToast.reset();
        ToastManager.resetForTesting();
    }

    private void assertToastShown(String message) {
        assertTrue(
                "Toast with message '" + message + "' was not shown as expected.",
                ShadowToast.showedCustomToast(message, R.id.toast_text));
    }

    @Test
    public void testShowManagedByAdministratorToast() {
        ManagedPreferencesUtils.showManagedByAdministratorToast(mActivity);
        assertToastShown(mActivity.getString(R.string.managed_by_your_organization));
    }

    @Test
    public void testShowManagedByParentToastNullDelegate() {
        ManagedPreferencesUtils.showManagedByParentToast(mActivity, null);
        assertToastShown(mActivity.getString(R.string.managed_by_your_parent));
    }

    @Test
    public void testShowManagedByParentToastSingleCustodian() {
        ManagedPreferencesUtils.showManagedByParentToast(
                mActivity, ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE);
        assertToastShown(mActivity.getString(R.string.managed_by_your_parent));
    }

    @Test
    public void testShowManagedByParentToastMultipleCustodians() {
        ManagedPreferencesUtils.showManagedByParentToast(
                mActivity, ManagedPreferenceTestDelegates.MULTI_CUSTODIAN_DELEGATE);
        assertToastShown(mActivity.getString(R.string.managed_by_your_parents));
    }

    @Test
    public void testShowManagedSettingsCannotBeResetToast() {
        ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(mActivity);
        assertToastShown(mActivity.getString(R.string.managed_settings_cannot_be_reset));
    }

    @Test
    public void testShowRecommendationToast() {
        ManagedPreferencesUtils.showRecommendationToast(mActivity);
        assertToastShown(mActivity.getString(R.string.recommended_by_your_organization));
    }

    @Test
    public void testGetManagedIconResId_Null() {
        int actual = ManagedPreferencesUtils.getManagedIconResId(null, mPreference);
        Assert.assertEquals("Incorrect icon displayed.", 0, actual);
    }

    @Test
    public void testGetManagedIconResId_Unmanaged() {
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE, mPreference);
        Assert.assertEquals("Incorrect icon displayed.", 0, actual);
    }

    @Test
    public void testGetManagedIconResId_Policy() {
        int expected = ManagedPreferencesUtils.getManagedByEnterpriseIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.POLICY_DELEGATE, mPreference);
        Assert.assertEquals("Incorrect icon displayed.", expected, actual);
    }

    @Test
    public void testGetManagedIconResId_Custodian() {
        int expected = ManagedPreferencesUtils.getManagedByCustodianIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE, mPreference);
        Assert.assertEquals("Incorrect icon displayed.", expected, actual);
    }

    @Test
    public void testGetManagedIconResId_Recommended() {
        int expected = ManagedPreferencesUtils.getManagedByEnterpriseIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_FOLLOWING, mPreference);
        Assert.assertEquals("Incorrect icon displayed.", expected, actual);
    }

    @Test
    public void testGetManagedIconResId_RecommendedOverridden() {
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_OVERRIDDEN,
                        mPreference);
        Assert.assertEquals("No icon should be shown for an overridden recommendation.", 0, actual);
    }

    @Test
    public void testGetManagedIconResId_PolicyAndRecommended() {
        // Policy should take precedence and show the enterprise icon.
        int expected = ManagedPreferencesUtils.getManagedByEnterpriseIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.POLICY_AND_RECOMMENDED_DELEGATE,
                        mPreference);
        Assert.assertEquals("Incorrect icon displayed.", expected, actual);
    }

    @Test
    public void testGetManagedIconResId_CustodianAndRecommended() {
        // Custodian should take precedence and show the custodian icon.
        int expected = ManagedPreferencesUtils.getManagedByCustodianIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.CUSTODIAN_AND_RECOMMENDED_DELEGATE,
                        mPreference);
        Assert.assertEquals("Incorrect icon displayed.", expected, actual);
    }

    @Test
    public void testGetManagedIconResId_AllManaged() {
        // Policy should take precedence over all and show the enterprise icon.
        int expected = ManagedPreferencesUtils.getManagedByEnterpriseIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.ALL_MANAGED_DELEGATE, mPreference);
        Assert.assertEquals("Incorrect icon displayed.", expected, actual);
    }

    @Test
    public void testInitPreference_SetsIconForCustomLayout() {
        // This test now verifies a preference with a custom layout and expects the icon to be set
        // directly on the Preference object.
        ManagedPreferencesUtils.initPreference(
                ManagedPreferenceTestDelegates.POLICY_DELEGATE,
                mPreference,
                /* allowManagedIcon= */ true,
                /* hasCustomLayout= */ true);
        assertNotNull("Managed icon should be set for custom layouts.", mPreference.getIcon());
    }

    @Test
    public void testInitPreference_DoesNotSetIconWhenDisallowed() {
        ManagedPreferencesUtils.initPreference(
                ManagedPreferenceTestDelegates.POLICY_DELEGATE,
                mPreference,
                /* allowManagedIcon= */ false,
                /* hasCustomLayout= */ false);
        assertNull("Managed icon should not be set.", mPreference.getIcon());
    }

    @Test
    public void testInitPreference_SetsDefaultLayout() {
        int initialLayout = mPreference.getLayoutResource();
        ManagedPreferencesUtils.initPreference(
                ManagedPreferenceTestDelegates.POLICY_DELEGATE,
                mPreference,
                /* allowManagedIcon= */ false,
                /* hasCustomLayout= */ false);
        int finalLayout = mPreference.getLayoutResource();
        Assert.assertNotEquals("Layout should have changed.", initialLayout, finalLayout);
        assertEquals(
                "Default layout for managed preference was not set.",
                ManagedPreferenceTestDelegates.POLICY_DELEGATE.defaultPreferenceLayoutResource(),
                finalLayout);
    }

    @Test
    public void testInitPreference_DoesNotSetDefaultLayoutWithCustomLayout() {
        int initialLayout = mPreference.getLayoutResource();
        ManagedPreferencesUtils.initPreference(
                ManagedPreferenceTestDelegates.POLICY_DELEGATE,
                mPreference,
                /* allowManagedIcon= */ false,
                /* hasCustomLayout= */ true);
        assertEquals(
                "Default layout should not be set when preference has a custom layout.",
                initialLayout,
                mPreference.getLayoutResource());
    }

    @Test
    public void testOnBindViewToPreference_SummaryUpdate() {
        View view = LayoutInflater.from(mActivity).inflate(R.layout.preference, null);
        @SuppressLint("RestrictedApi")
        PreferenceViewHolder holder = createInstanceForTests(view);
        mPreference.setSummary("Original Summary");

        // First, bind the original preference data to the view.
        mPreference.onBindViewHolder(holder);

        // Then, apply the managed state modifications.
        ManagedPreferencesUtils.onBindViewToPreference(
                ManagedPreferenceTestDelegates.POLICY_DELEGATE, mPreference, holder.itemView);

        TextView summaryView = holder.itemView.findViewById(android.R.id.summary);
        String expected =
                "Original Summary\n" + mActivity.getString(R.string.managed_by_your_organization);
        assertEquals("Summary text is incorrect.", expected, summaryView.getText().toString());
        assertEquals(View.VISIBLE, summaryView.getVisibility());
    }

    @Test
    public void testOnBindViewToPreference_RecommendedFollowing() {
        View view = LayoutInflater.from(mActivity).inflate(R.layout.preference, null);
        @SuppressLint("RestrictedApi")
        PreferenceViewHolder holder = createInstanceForTests(view);
        mPreference.setSummary("Original Summary");

        // First, bind the original preference data to the view.
        mPreference.onBindViewHolder(holder);

        // Then, apply the managed state modifications.
        ManagedPreferencesUtils.onBindViewToPreference(
                ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_FOLLOWING,
                mPreference,
                holder.itemView);

        TextView summaryView = holder.itemView.findViewById(android.R.id.summary);
        // Assumes the layout does not contain R.id.managed_disclaimer_text, which triggers
        // the legacy behavior of appending the disclaimer to the summary.
        String expected =
                "Original Summary\n"
                        + mActivity.getString(R.string.recommended_by_your_organization);
        assertEquals(
                "Summary text is incorrect for a followed recommendation.",
                expected,
                summaryView.getText().toString());
        assertEquals(View.VISIBLE, summaryView.getVisibility());
    }

    @Test
    public void testOnBindViewToPreference_RecommendedOverridden() {
        View view = LayoutInflater.from(mActivity).inflate(R.layout.preference, null);
        @SuppressLint("RestrictedApi")
        PreferenceViewHolder holder = createInstanceForTests(view);
        mPreference.setSummary("Original Summary");

        // First, bind the original preference data to the view.
        mPreference.onBindViewHolder(holder);

        // Then, apply the managed state modifications.
        ManagedPreferencesUtils.onBindViewToPreference(
                ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_OVERRIDDEN,
                mPreference,
                holder.itemView);

        TextView summaryView = holder.itemView.findViewById(android.R.id.summary);
        assertEquals(
                "Summary text should not be changed for an overridden recommendation.",
                "Original Summary",
                summaryView.getText().toString());
        assertEquals(View.VISIBLE, summaryView.getVisibility());
    }

    private void testPreferenceEnabledState(
            ManagedPreferenceDelegate delegate, boolean shouldBeEnabled) {
        // Test initPreference()
        Preference preference = new Preference(mActivity);
        ManagedPreferencesUtils.initPreference(
                delegate, preference, /* allowManagedIcon= */ false, /* hasCustomLayout= */ false);
        assertEquals(
                "Preference enabled state is incorrect after initPreference for "
                        + delegate.getClass().getSimpleName(),
                shouldBeEnabled,
                preference.isEnabled());

        // Test onBindViewToPreference()
        View view = LayoutInflater.from(mActivity).inflate(R.layout.preference, null);
        @SuppressLint("RestrictedApi")
        PreferenceViewHolder holder = createInstanceForTests(view);
        // `setEnabled` state from initPreference carries over to the holder.
        preference.onBindViewHolder(holder);
        // The util call further disables the view recursively if needed.
        ManagedPreferencesUtils.onBindViewToPreference(delegate, preference, holder.itemView);
        assertEquals(
                "View enabled state is incorrect after onBindViewToPreference for "
                        + delegate.getClass().getSimpleName(),
                shouldBeEnabled,
                holder.itemView.isEnabled());
    }

    @Test
    public void testEnabledState_Policy() {
        testPreferenceEnabledState(ManagedPreferenceTestDelegates.POLICY_DELEGATE, false);
    }

    @Test
    public void testEnabledState_Custodian() {
        testPreferenceEnabledState(ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE, false);
    }

    @Test
    public void testEnabledState_RecommendedFollowing() {
        testPreferenceEnabledState(
                ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_FOLLOWING, true);
    }

    @Test
    public void testEnabledState_RecommendedOverridden() {
        testPreferenceEnabledState(
                ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_OVERRIDDEN, true);
    }

    @Test
    public void testEnabledState_Unmanaged() {
        testPreferenceEnabledState(ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE, true);
    }

    @Test
    public void testEnabledState_PolicyAndRecommended() {
        // Enforced policy takes precedence and should disable the preference.
        testPreferenceEnabledState(
                ManagedPreferenceTestDelegates.POLICY_AND_RECOMMENDED_DELEGATE, false);
    }

    @Test
    public void testEnabledState_CustodianAndRecommended() {
        // Custodian control takes precedence and should disable the preference.
        testPreferenceEnabledState(
                ManagedPreferenceTestDelegates.CUSTODIAN_AND_RECOMMENDED_DELEGATE, false);
    }

    @Test
    public void testEnabledState_AllManaged() {
        // Enforced policy takes highest precedence and should disable the preference.
        testPreferenceEnabledState(ManagedPreferenceTestDelegates.ALL_MANAGED_DELEGATE, false);
    }

    private void testWidgetIconClick(
            ManagedPreferenceDelegate delegate, @Nullable String expectedToast) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create a preference that has an image widget.
                    ChromeImageViewPreference preference = new ChromeImageViewPreference(mActivity);
                    preference.setManagedPreferenceDelegate(delegate);

                    LayoutInflater inflater = LayoutInflater.from(mActivity);
                    // Inflate the main preference layout which contains the widget_frame.
                    View view = inflater.inflate(R.layout.preference, null);

                    ViewGroup widgetFrame = view.findViewById(android.R.id.widget_frame);
                    assertNotNull("widget_frame not found in layout", widgetFrame);
                    inflater.inflate(preference.getWidgetLayoutResource(), widgetFrame);

                    // Now that the view hierarchy is complete, create the holder and bind it.
                    @SuppressLint("RestrictedApi")
                    PreferenceViewHolder holder = createInstanceForTests(view);
                    preference.onBindViewHolder(holder);

                    // Find the widget and simulate a click.
                    ImageView widget = view.findViewById(R.id.image_view_widget);
                    assertNotNull("Image view widget should be present in the layout.", widget);

                    if (expectedToast != null) {
                        assertEquals(
                                "Content description is incorrect.",
                                expectedToast,
                                widget.getContentDescription());
                        assertTrue("Widget should be clickable.", widget.isClickable());
                    }

                    widget.performClick();
                });

        // Check if the correct toast was shown, or if no toast was shown.
        if (expectedToast != null) {
            assertToastShown(expectedToast);
        } else {
            assertEquals(
                    "A toast was shown when none was expected.", 0, ShadowToast.shownToastCount());
        }
    }

    @Test
    public void testWidgetIconClick_Unmanaged() {
        testWidgetIconClick(ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE, null);
    }

    @Test
    public void testWidgetIconClick_RecommendedOverridden() {
        testWidgetIconClick(ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_OVERRIDDEN, null);
    }

    @Test
    public void testWidgetIconClick_RecommendedFollowing() {
        testWidgetIconClick(
                ManagedPreferenceTestDelegates.RECOMMENDED_DELEGATE_FOLLOWING,
                mActivity.getString(R.string.recommended_by_your_organization));
    }

    @Test
    public void testWidgetIconClick_PolicyAndRecommended() {
        // Enforced policy should take precedence over recommendation.
        testWidgetIconClick(
                ManagedPreferenceTestDelegates.POLICY_AND_RECOMMENDED_DELEGATE,
                mActivity.getString(R.string.managed_by_your_organization));
    }

    @Test
    public void testWidgetIconClick_CustodianAndRecommended() {
        // Custodian control should take precedence over recommendation.
        testWidgetIconClick(
                ManagedPreferenceTestDelegates.CUSTODIAN_AND_RECOMMENDED_DELEGATE,
                mActivity.getString(R.string.managed_by_your_parent));
    }

    @Test
    public void testWidgetIconClick_AllManaged() {
        // Enforced policy should take precedence over all other states.
        testWidgetIconClick(
                ManagedPreferenceTestDelegates.ALL_MANAGED_DELEGATE,
                mActivity.getString(R.string.managed_by_your_organization));
    }

    @Test
    public void testWidgetIconClick_Policy() {
        testWidgetIconClick(
                ManagedPreferenceTestDelegates.POLICY_DELEGATE,
                mActivity.getString(R.string.managed_by_your_organization));
    }

    @Test
    public void testWidgetIconClick_Custodian() {
        testWidgetIconClick(
                ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE,
                mActivity.getString(R.string.managed_by_your_parent));
    }

    @Test
    public void testOnClickPreference_Unmanaged() {
        boolean wasClickHandled =
                ManagedPreferencesUtils.onClickPreference(
                        ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE, mPreference);

        assertFalse(
                "onClickPreference should return false for unmanaged preference.", wasClickHandled);
        assertEquals(
                "No toast should be shown for an unmanaged preference.",
                0,
                ShadowToast.shownToastCount());
    }

    @Test
    public void testOnClickPreference_Policy() {
        boolean wasClickHandled =
                ManagedPreferencesUtils.onClickPreference(
                        ManagedPreferenceTestDelegates.POLICY_DELEGATE, mPreference);

        assertTrue(
                "onClickPreference should return true for policy-managed preference.",
                wasClickHandled);
        assertToastShown(mActivity.getString(R.string.managed_by_your_organization));
    }

    @Test
    public void testOnClickPreference_Custodian() {
        boolean wasClickHandled =
                ManagedPreferencesUtils.onClickPreference(
                        ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE, mPreference);

        assertTrue(
                "onClickPreference should return true for custodian-managed preference.",
                wasClickHandled);
        assertToastShown(mActivity.getString(R.string.managed_by_your_parent));
    }
}
