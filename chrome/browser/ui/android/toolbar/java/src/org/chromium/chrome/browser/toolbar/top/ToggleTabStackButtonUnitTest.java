// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link ToggleTabStackButton}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class ToggleTabStackButtonUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(0);
    private final ObservableSupplierImpl<TabModelDotInfo> mTabModelDotInfoSupplier =
            new ObservableSupplierImpl<>(TabModelDotInfo.HIDE);
    private final Supplier<Boolean> mIsIncognitoSupplier = () -> false;

    @Mock private UserEducationHelper mUserEducationHelper;
    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private Activity mActivity;
    private ToggleTabStackButton mToggleTabStackButton;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mToggleTabStackButton = new ToggleTabStackButton(mActivity, null);
        mToggleTabStackButton.onFinishInflate();
    }

    @Test
    public void testTabModelDotInfoIph() {
        mToggleTabStackButton.setSuppliers(
                mTabCountSupplier,
                mTabModelDotInfoSupplier,
                mIsIncognitoSupplier,
                mUserEducationHelper);
        ShadowLooper.idleMainLooper();

        String groupTitle = "Vacation";
        mTabModelDotInfoSupplier.set(new TabModelDotInfo(true, groupTitle));
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        assertTrue(mIphCommandCaptor.getValue().contentString.contains(groupTitle));
    }
}
