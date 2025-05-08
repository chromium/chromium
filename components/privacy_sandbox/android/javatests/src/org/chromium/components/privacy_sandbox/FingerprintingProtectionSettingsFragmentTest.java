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

import static org.chromium.components.privacy_sandbox.FingerprintingProtectionSettingsFragment.FP_PROTECTION_PREF_HISTOGRAM_NAME;

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

/** Tests for {@link FingerprintingProtectionSettingsFragment}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class FingerprintingProtectionSettingsFragmentTest {
    private static final int PREF_TOGGLE_LABEL =
            R.string.incognito_tracking_protections_fingerprinting_protection_toggle_label;
    private static final int PREF_TOGGLE_SUBLABEL =
            R.string.incognito_tracking_protections_fingerprinting_protection_toggle_sublabel;
    private static final int WHEN_ON =
            R.string.incognito_tracking_protections_fingerprinting_protection_when_on;
    private static final int THINGS_TO_CONSIDER =
            R.string.incognito_tracking_protections_fingerprinting_protection_things_to_consider;

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private TrackingProtectionDelegate mDelegate;

    private FingerprintingProtectionSettingsFragment mFragment;

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
                FingerprintingProtectionSettingsFragment.class,
                null,
                (fragment) ->
                        ((FingerprintingProtectionSettingsFragment) fragment)
                                .setTrackingProtectionDelegate(mDelegate));
        mFragment =
                (FingerprintingProtectionSettingsFragment) mSettingsRule.getPreferenceFragment();
    }

    @Test
    @SmallTest
    public void showFpProtectionUi() {
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        onView(allOf(withText(PREF_TOGGLE_LABEL), hasSibling(withText(PREF_TOGGLE_SUBLABEL))))
                .check(matches(isDisplayed()));
        onView(withText(WHEN_ON)).check(matches(isDisplayed()));
        onView(withText(THINGS_TO_CONSIDER)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void enablingFpProtectionToggleUpdatesPrefAndRecordsHistogram() {
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(false);
        doNothing().when(mDelegate).setFingerprintingProtection(anyBoolean());
        HistogramWatcher fingerprintingProtectionHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(FP_PROTECTION_PREF_HISTOGRAM_NAME, true)
                        .build();

        launchTrackingProtectionSettings();

        onView(allOf(withText(PREF_TOGGLE_LABEL), hasSibling(withText(PREF_TOGGLE_SUBLABEL))))
                .perform(click());
        verify(mDelegate).setFingerprintingProtection(true);
        fingerprintingProtectionHistogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void disablingFpProtectionToggleUpdatesPrefAndRecordsHistogram() {
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(true);
        doNothing().when(mDelegate).setFingerprintingProtection(anyBoolean());
        HistogramWatcher fingerprintingProtectionHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(FP_PROTECTION_PREF_HISTOGRAM_NAME, false)
                        .build();

        launchTrackingProtectionSettings();

        onView(allOf(withText(PREF_TOGGLE_LABEL), hasSibling(withText(PREF_TOGGLE_SUBLABEL))))
                .perform(click());
        verify(mDelegate).setFingerprintingProtection(false);
        fingerprintingProtectionHistogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void fpProtectionToggleIsManagedWhenFpProtectionIsManaged() {
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(true);
        when(mDelegate.isFingerprintingProtectionManaged()).thenReturn(true);

        launchTrackingProtectionSettings();

        String fppSublabel = mFragment.getContext().getString(PREF_TOGGLE_SUBLABEL);
        String enterpriseSublabel =
                mFragment.getContext().getString(R.string.managed_by_your_organization);
        onView(
                allOf(
                        withText(PREF_TOGGLE_LABEL),
                        hasSibling(withText(containsString(fppSublabel))),
                        isDisplayed()));
        onView(withText(containsString(enterpriseSublabel))).check(matches(isDisplayed()));
    }
}
