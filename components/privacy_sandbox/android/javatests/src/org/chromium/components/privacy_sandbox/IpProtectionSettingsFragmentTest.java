// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.Matchers.allOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.privacy_sandbox.IpProtectionSettingsFragment.IP_PROTECTION_PREF_HISTOGRAM_NAME;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.ManagedPreferenceTestDelegates;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;

/** Tests for {@link IpProtectionSettingsFragment}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class IpProtectionSettingsFragmentTest {
    private static final int PREF_TOGGLE_LABEL =
            R.string.incognito_tracking_protections_ip_protection_toggle_label;
    private static final int PREF_TOGGLE_SUBLABEL =
            R.string.incognito_tracking_protections_ip_protection_toggle_sublabel;

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private TrackingProtectionDelegate mDelegate;

    private IpProtectionSettingsFragment mFragment;

    @BeforeClass
    public static void setUpSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        SiteSettingsDelegate mockDelegate = mock(SiteSettingsDelegate.class);
        when(mDelegate.getSiteSettingsDelegate(any())).thenReturn(mockDelegate);
        when(mockDelegate.getManagedPreferenceDelegate())
                .thenReturn(ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE);
    }

    private void launchTrackingProtectionSettings() {
        mSettingsRule.launchPreference(
                IpProtectionSettingsFragment.class,
                null,
                (fragment) ->
                        ((IpProtectionSettingsFragment) fragment)
                                .setTrackingProtectionDelegate(mDelegate));
        mFragment = (IpProtectionSettingsFragment) mSettingsRule.getPreferenceFragment();
    }

    @Test
    @SmallTest
    public void showIpProtectionUi() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        onView(allOf(withText(PREF_TOGGLE_LABEL), hasSibling(withText(PREF_TOGGLE_SUBLABEL))))
                .check(matches(isDisplayed()));

        onView(withText(R.string.incognito_tracking_protections_ip_protection_when_on))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                R.string
                                        .incognito_tracking_protections_ip_protection_things_to_consider_bullet_one))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                R.string
                                        .incognito_tracking_protections_ip_protection_things_to_consider_bullet_one))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                R.string
                                        .incognito_tracking_protections_ip_protection_things_to_consider_bullet_one))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void enablingIpProtectionToggleUpdatesPrefAndRecordsHistogram() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(false);
        doNothing().when(mDelegate).setIpProtection(anyBoolean());
        HistogramWatcher ipProtectionHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(IP_PROTECTION_PREF_HISTOGRAM_NAME, true)
                        .build();

        launchTrackingProtectionSettings();

        onView(
                        allOf(
                                withText(PREF_TOGGLE_LABEL),
                                hasSibling(withText(PREF_TOGGLE_SUBLABEL)),
                                isDisplayed()))
                .perform(click());
        verify(mDelegate).setIpProtection(true);
        ipProtectionHistogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void disablingIpProtectionToggleUpdatesPrefAndRecordsHistogram() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);
        doNothing().when(mDelegate).setIpProtection(anyBoolean());
        HistogramWatcher ipProtectionHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(IP_PROTECTION_PREF_HISTOGRAM_NAME, false)
                        .build();

        launchTrackingProtectionSettings();

        onView(
                        allOf(
                                withText(PREF_TOGGLE_LABEL),
                                hasSibling(withText(PREF_TOGGLE_SUBLABEL)),
                                isDisplayed()))
                .perform(click());
        verify(mDelegate).setIpProtection(false);
        ipProtectionHistogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void ipProtectionToggleIsManagedWhenIpProtectionIsDisabledForEnterprise() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);
        when(mDelegate.isIpProtectionDisabledForEnterprise()).thenReturn(true);
        SiteSettingsDelegate mockDelegate = mock(SiteSettingsDelegate.class);
        when(mDelegate.getSiteSettingsDelegate(any())).thenReturn(mockDelegate);
        when(mockDelegate.getManagedPreferenceDelegate())
                .thenReturn(ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE);

        launchTrackingProtectionSettings();

        String ippSublabel = mFragment.getContext().getString(PREF_TOGGLE_SUBLABEL);
        String enterpriseSublabel =
                mFragment.getContext().getString(R.string.managed_by_your_organization);
        onView(
                allOf(
                        withText(PREF_TOGGLE_LABEL),
                        hasSibling(withText(containsString(ippSublabel))),
                        isDisplayed()));
        onView(withText(containsString(enterpriseSublabel))).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void ipProtectionToggleIsManagedWhenIpProtectionIsManaged() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);
        when(mDelegate.isIpProtectionManaged()).thenReturn(true);

        launchTrackingProtectionSettings();

        String ippSublabel = mFragment.getContext().getString(PREF_TOGGLE_SUBLABEL);
        String enterpriseSublabel =
                mFragment.getContext().getString(R.string.managed_by_your_organization);
        onView(
                allOf(
                        withText(PREF_TOGGLE_LABEL),
                        hasSibling(withText(containsString(ippSublabel))),
                        isDisplayed()));
        onView(withText(containsString(enterpriseSublabel))).check(matches(isDisplayed()));
    }
}
