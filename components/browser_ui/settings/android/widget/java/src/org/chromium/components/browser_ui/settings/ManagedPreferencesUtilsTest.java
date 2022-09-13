// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import android.support.test.InstrumentationRegistry;

import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.widget.Toast;

/**
 * Tests of {@link ManagedPreferencesUtils}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ManagedPreferencesUtilsTest extends BlankUiTestActivityTestCase {
    public static final ManagedPreferenceDelegate UNMANAGED_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return false;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }
            };

    public static final ManagedPreferenceDelegate POLICY_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return true;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return false;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }
            };

    public static final ManagedPreferenceDelegate SINGLE_CUSTODIAN_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return true;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }
            };

    public static final ManagedPreferenceDelegate MULTI_CUSTODIAN_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return true;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return true;
                }
            };

    @Test
    @SmallTest
    public void testShowManagedByAdministratorToast() {
        Toast toast = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ManagedPreferencesUtils.showManagedByAdministratorToast(getActivity());
        });

        onView(withText(R.string.managed_by_your_organization))
                .inRoot(withDecorView(not(getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> toast.cancel());
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastNullDelegate() {
        Toast toast = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ManagedPreferencesUtils.showManagedByParentToast(getActivity(), null);
        });

        onView(withText(R.string.managed_by_your_parent))
                .inRoot(withDecorView(not(getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> toast.cancel());
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastSingleCustodian() {
        Toast toast = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ManagedPreferencesUtils.showManagedByParentToast(
                    getActivity(), SINGLE_CUSTODIAN_DELEGATE);
        });

        onView(withText(R.string.managed_by_your_parent))
                .inRoot(withDecorView(not(getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> toast.cancel());
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastMultipleCustodians() {
        Toast toast = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ManagedPreferencesUtils.showManagedByParentToast(
                    getActivity(), MULTI_CUSTODIAN_DELEGATE);
        });

        onView(withText(R.string.managed_by_your_parents))
                .inRoot(withDecorView(not(getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> toast.cancel());
    }

    @Test
    @SmallTest
    public void testShowManagedSettingsCannotBeResetToast() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(getActivity());
        });

        onView(withText(R.string.managed_settings_cannot_be_reset))
                .inRoot(withDecorView(not(getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdNull() {
        Preference pref = new Preference(InstrumentationRegistry.getTargetContext());
        int actual = ManagedPreferencesUtils.getManagedIconResId(null, pref);
        Assert.assertEquals(0, actual);
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdPolicy() {
        Preference pref = new Preference(InstrumentationRegistry.getTargetContext());
        int expected = ManagedPreferencesUtils.getManagedByEnterpriseIconId();
        int actual = ManagedPreferencesUtils.getManagedIconResId(POLICY_DELEGATE, pref);
        Assert.assertEquals(expected, actual);
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdCustodian() {
        Preference pref = new Preference(InstrumentationRegistry.getTargetContext());
        int expected = ManagedPreferencesUtils.getManagedByCustodianIconId();
        int actual = ManagedPreferencesUtils.getManagedIconResId(SINGLE_CUSTODIAN_DELEGATE, pref);
        Assert.assertEquals(expected, actual);
    }
}
