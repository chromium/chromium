// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import static org.mockito.Mockito.doReturn;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class ManagementViewTest {
    private static final String TITLE = "title";

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock NativePageHost mMockNativePageHost;
    @Mock ManagedBrowserUtils.Natives mMockManagedBrowserUtilNatives;

    @Mock Profile mMockProfile;
    @Mock UserPrefs.Natives mMockUserPrefsNatives;
    @Mock PrefService mMockPrefService;

    ManagementCoordinator mCoordinator;

    TestActivity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(ManagedBrowserUtilsJni.TEST_HOOKS, mMockManagedBrowserUtilNatives);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsNatives);
        doReturn(mMockPrefService).when(mMockUserPrefsNatives).get(mMockProfile);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            doReturn(activity).when(mMockNativePageHost).getContext();
                        });
    }

    private void createDialog() {
        mCoordinator = new ManagementCoordinator(mMockNativePageHost, mMockProfile);
        mActivity.setContentView(mCoordinator.getView());
    }

    @Test
    public void testNotManaged() {
        doReturn(TITLE).when(mMockManagedBrowserUtilNatives).getTitle(mMockProfile);
        doReturn(false).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(false).when(mMockManagedBrowserUtilNatives).isBrowserReportingEnabled();
        doReturn(false)
                .when(mMockPrefService)
                .isManagedPreference(Pref.CLOUD_LEGACY_TECH_REPORT_ALLOWLIST);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();
        Assert.assertNotNull(view.mTitle);
        Assert.assertEquals(TITLE, view.mTitle.getText());
        Assert.assertEquals(View.VISIBLE, view.mTitle.getVisibility());

        Assert.assertEquals(View.GONE, view.mDescription.getVisibility());
        Assert.assertEquals(View.GONE, view.mLearnMore.getVisibility());
        Assert.assertEquals(View.GONE, view.mBrowserReporting.getVisibility());
        Assert.assertEquals(View.GONE, view.mBrowserReportingExplanation.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportUsername.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportVersion.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportLegacyTech.getVisibility());
    }

    @Test
    public void testManaged() {
        doReturn(TITLE).when(mMockManagedBrowserUtilNatives).getTitle(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileManaged(mMockProfile);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();
        Assert.assertNotNull(view.mTitle);
        Assert.assertEquals(TITLE, view.mTitle.getText());
        Assert.assertEquals(View.VISIBLE, view.mTitle.getVisibility());

        Assert.assertEquals(View.VISIBLE, view.mDescription.getVisibility());
        Assert.assertEquals(
                mActivity.getString(R.string.management_browser_notice),
                view.mDescription.getText());
        Assert.assertEquals(View.VISIBLE, view.mLearnMore.getVisibility());
    }

    @Test
    public void testOnlyBrowserManaged() {
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(false).when(mMockManagedBrowserUtilNatives).isProfileManaged(mMockProfile);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();
        Assert.assertEquals(View.VISIBLE, view.mDescription.getVisibility());
        Assert.assertEquals(
                mActivity.getString(R.string.management_browser_notice),
                view.mDescription.getText());
        Assert.assertEquals(View.VISIBLE, view.mLearnMore.getVisibility());
    }

    @Test
    public void testOnlyProfileManaged() {
        doReturn(false).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileManaged(mMockProfile);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();
        Assert.assertEquals(View.VISIBLE, view.mDescription.getVisibility());
        Assert.assertEquals(
                mActivity.getString(R.string.management_profile_notice),
                view.mDescription.getText());
        Assert.assertEquals(View.VISIBLE, view.mLearnMore.getVisibility());
    }

    @Test
    public void testBothCloudReporting() {
        doReturn(TITLE).when(mMockManagedBrowserUtilNatives).getTitle(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserReportingEnabled();
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileReportingEnabled(mMockProfile);
        doReturn(false)
                .when(mMockPrefService)
                .isManagedPreference(Pref.CLOUD_LEGACY_TECH_REPORT_ALLOWLIST);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();

        Assert.assertEquals(View.VISIBLE, view.mBrowserReporting.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mBrowserReportingExplanation.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mReportUsername.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mReportVersion.getVisibility());
        Assert.assertEquals(View.GONE, view.mProfileReportingExplanation.getVisibility());
        Assert.assertEquals(View.GONE, view.mProfileReportDetails.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportLegacyTech.getVisibility());
    }

    @Test
    public void testOnlyCloudBrowserReporting() {
        doReturn(TITLE).when(mMockManagedBrowserUtilNatives).getTitle(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserReportingEnabled();
        doReturn(false)
                .when(mMockManagedBrowserUtilNatives)
                .isProfileReportingEnabled(mMockProfile);
        doReturn(false)
                .when(mMockPrefService)
                .isManagedPreference(Pref.CLOUD_LEGACY_TECH_REPORT_ALLOWLIST);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();

        Assert.assertEquals(View.VISIBLE, view.mBrowserReporting.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mBrowserReportingExplanation.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mReportUsername.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mReportVersion.getVisibility());
        Assert.assertEquals(View.GONE, view.mProfileReportingExplanation.getVisibility());
        Assert.assertEquals(View.GONE, view.mProfileReportDetails.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportLegacyTech.getVisibility());
    }

    @Test
    public void testOnlyCloudProfileReporting() {
        doReturn(TITLE).when(mMockManagedBrowserUtilNatives).getTitle(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileManaged(mMockProfile);
        doReturn(false).when(mMockManagedBrowserUtilNatives).isBrowserReportingEnabled();
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileReportingEnabled(mMockProfile);
        doReturn(false)
                .when(mMockPrefService)
                .isManagedPreference(Pref.CLOUD_LEGACY_TECH_REPORT_ALLOWLIST);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();

        Assert.assertEquals(View.VISIBLE, view.mBrowserReporting.getVisibility());
        Assert.assertEquals(View.GONE, view.mBrowserReportingExplanation.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportUsername.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportVersion.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mProfileReportingExplanation.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mProfileReportDetails.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportLegacyTech.getVisibility());
    }

    @Test
    public void testLegacyReporting() {
        doReturn(TITLE).when(mMockManagedBrowserUtilNatives).getTitle(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isBrowserManaged(mMockProfile);
        doReturn(true).when(mMockManagedBrowserUtilNatives).isProfileManaged(mMockProfile);
        doReturn(false).when(mMockManagedBrowserUtilNatives).isBrowserReportingEnabled();
        doReturn(false)
                .when(mMockManagedBrowserUtilNatives)
                .isProfileReportingEnabled(mMockProfile);
        doReturn(true)
                .when(mMockPrefService)
                .isManagedPreference(Pref.CLOUD_LEGACY_TECH_REPORT_ALLOWLIST);

        createDialog();

        ManagementView view = (ManagementView) mCoordinator.getView();

        Assert.assertEquals(View.VISIBLE, view.mBrowserReporting.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mBrowserReportingExplanation.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportUsername.getVisibility());
        Assert.assertEquals(View.GONE, view.mReportVersion.getVisibility());
        Assert.assertEquals(View.VISIBLE, view.mReportLegacyTech.getVisibility());
    }
}
