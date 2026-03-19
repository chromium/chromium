// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

@RunWith(BaseRobolectricTestRunner.class)
public class ManagementPageUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private NativePageHost mMockNativePageHost;
    @Mock private Profile mMockProfile;
    @Mock private ManagedBrowserUtils.Natives mMockManagedBrowserUtilNatives;
    @Mock private UserPrefs.Natives mMockUserPrefsNatives;
    @Mock private PrefService mMockPrefService;
    @Mock private EdgeToEdgeController mMockEdgeToEdgeController;

    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier =
            ObservableSuppliers.createMonotonic();
    private ManagementPage mManagementPage;
    private Activity mActivity;

    @Before
    public void setup() {
        ManagedBrowserUtilsJni.setInstanceForTesting(mMockManagedBrowserUtilNatives);
        UserPrefsJni.setInstanceForTesting(mMockUserPrefsNatives);
        doReturn(mMockPrefService).when(mMockUserPrefsNatives).get(mMockProfile);

        mActivityScenarios.getScenario().onActivity(activity -> mActivity = activity);
        doReturn(mActivity).when(mMockNativePageHost).getContext();
        doReturn(null).when(mMockNativePageHost).createEdgeToEdgePadAdjuster(any());
        doAnswer(
                        invocation ->
                                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                                        invocation.getArgument(0), mEdgeToEdgeSupplier))
                .when(mMockNativePageHost)
                .createEdgeToEdgePadAdjuster(any());

        mManagementPage = new ManagementPage(mMockNativePageHost, mMockProfile);
    }

    @Test
    public void testEdgeToEdge() {
        assertTrue("ManagementPage should support E2E.", mManagementPage.supportsEdgeToEdge());

        mEdgeToEdgeSupplier.set(mMockEdgeToEdgeController);
        verify(mMockEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        EdgeToEdgePadAdjuster padAdjuster = mPadAdjusterCaptor.getValue();

        ManagementView view = (ManagementView) mManagementPage.getView();
        int initialPaddingBottom = view.getPaddingBottom();

        padAdjuster.overrideBottomInset(100);
        assertEquals(
                "Bottom padding should be updated.",
                initialPaddingBottom + 100,
                view.getPaddingBottom());
        assertFalse("clipToPadding should be false.", view.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals(
                "Bottom padding should be reset.", initialPaddingBottom, view.getPaddingBottom());
        assertTrue("clipToPadding should be true.", view.getClipToPadding());

        mManagementPage.destroy();
        verify(mMockEdgeToEdgeController).unregisterAdjuster(padAdjuster);
    }
}
