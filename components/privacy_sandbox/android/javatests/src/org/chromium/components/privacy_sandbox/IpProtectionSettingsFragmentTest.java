// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doNothing;
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
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Tests for {@link IpProtectionSettingsFragment}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class IpProtectionSettingsFragmentTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private TrackingProtectionDelegate mDelegate;

    private IpProtectionSettingsFragment mFragment;

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
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
    public void testShowIpProtectionUi() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        String ipProtectionSummarySpanned =
                getIpProtectionSummarySpanned(
                        mFragment
                                .getResources()
                                .getString(R.string.privacy_sandbox_ip_protection_summary));

        onView(withText(ipProtectionSummarySpanned)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testRecordTrueForIpProtectionPerfHistogram() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(false);
        doNothing().when(mDelegate).setIpProtection(anyBoolean());
        HistogramWatcher ipProtectionHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(IP_PROTECTION_PREF_HISTOGRAM_NAME, true)
                        .build();

        launchTrackingProtectionSettings();

        String ipProtectionSummarySpanned =
                getIpProtectionSummarySpanned(
                        mFragment
                                .getResources()
                                .getString(R.string.privacy_sandbox_ip_protection_summary));
        onView(withText(ipProtectionSummarySpanned)).check(matches(isDisplayed()));

        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);
        onView(allOf(withText(R.string.text_off), isDisplayed())).perform(click());
        // checks whether the histogram was properly recorded
        ipProtectionHistogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFalseForIpProtectionPerfHistogram() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);
        doNothing().when(mDelegate).setIpProtection(anyBoolean());
        HistogramWatcher ipProtectionHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(IP_PROTECTION_PREF_HISTOGRAM_NAME, false)
                        .build();

        launchTrackingProtectionSettings();

        String ipProtectionSummarySpanned =
                getIpProtectionSummarySpanned(
                        mFragment
                                .getResources()
                                .getString(R.string.privacy_sandbox_ip_protection_summary));
        onView(withText(ipProtectionSummarySpanned)).check(matches(isDisplayed()));

        when(mDelegate.isIpProtectionEnabled()).thenReturn(false);
        onView(allOf(withText(R.string.text_on), isDisplayed())).perform(click());
        // checks whether the histogram was properly recorded
        ipProtectionHistogramWatcher.assertExpected();
    }

    private String getIpProtectionSummarySpanned(String mFragment) {
        return SpanApplier.applySpans(mFragment, new SpanInfo("<link>", "</link>", new Object()))
                .toString();
    }
}
