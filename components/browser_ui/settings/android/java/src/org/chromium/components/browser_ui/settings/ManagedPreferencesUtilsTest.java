// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.test.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ToastManager;

/** Tests of {@link ManagedPreferencesUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
@Batch(Batch.PER_CLASS)
public class ManagedPreferencesUtilsTest {
    private Activity mActivity;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
    }

    @After
    public void tearDown() {
        ShadowToast.reset();
        ToastManager.resetForTesting();
    }

    @Test
    @SmallTest
    public void testShowManagedByAdministratorToast() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ManagedPreferencesUtils.showManagedByAdministratorToast(mActivity);
                });

        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(R.string.managed_by_your_organization),
                        R.id.toast_text));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastNullDelegate() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ManagedPreferencesUtils.showManagedByParentToast(mActivity, null);
                });

        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(R.string.managed_by_your_parent),
                        R.id.toast_text));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastSingleCustodian() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ManagedPreferencesUtils.showManagedByParentToast(
                            mActivity, ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE);
                });

        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(R.string.managed_by_your_parent),
                        R.id.toast_text));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastMultipleCustodians() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ManagedPreferencesUtils.showManagedByParentToast(
                            mActivity, ManagedPreferenceTestDelegates.MULTI_CUSTODIAN_DELEGATE);
                });

        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(R.string.managed_by_your_parents),
                        R.id.toast_text));
    }

    @Test
    @SmallTest
    public void testShowManagedSettingsCannotBeResetToast() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(mActivity);
                });

        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity
                                .getResources()
                                .getString(R.string.managed_settings_cannot_be_reset),
                        R.id.toast_text));
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdNull() {
        Preference pref = new Preference(mActivity);
        int actual = ManagedPreferencesUtils.getManagedIconResId(null, pref);
        Assert.assertEquals(0, actual);
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdPolicy() {
        Preference pref = new Preference(mActivity);
        int expected = ManagedPreferencesUtils.getManagedByEnterpriseIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.POLICY_DELEGATE, pref);
        Assert.assertEquals(expected, actual);
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdCustodian() {
        Preference pref = new Preference(mActivity);
        int expected = ManagedPreferencesUtils.getManagedByCustodianIconId();
        int actual =
                ManagedPreferencesUtils.getManagedIconResId(
                        ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE, pref);
        Assert.assertEquals(expected, actual);
    }
}
