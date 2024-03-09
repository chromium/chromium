// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.when;

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

    @Mock private IpProtectionDelegate mDelegate;

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
                                .setIProtectionDelegate(mDelegate));
        mFragment = (IpProtectionSettingsFragment) mSettingsRule.getPreferenceFragment();
    }

    @Test
    @SmallTest
    public void testShowIpProtectionUi() {
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        String ipProtectionSummarySpanned =
                SpanApplier.applySpans(
                                mFragment
                                        .getResources()
                                        .getString(R.string.privacy_sandbox_ip_protection_summary),
                                new SpanInfo("<link>", "</link>", new Object()))
                        .toString();

        onView(withText(ipProtectionSummarySpanned)).check(matches(isDisplayed()));
    }
}
