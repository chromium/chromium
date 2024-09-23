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
import static org.mockito.Mockito.verify;
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

/** Tests for {@link FingerprintingProtectionSettingsFragment}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class FingerprintingProtectionSettingsFragmentTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private TrackingProtectionDelegate mDelegate;

    private FingerprintingProtectionSettingsFragment mFragment;

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
    public void testShowFpProtectionUi() {
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        onView(withText(R.string.tracking_protection_fingerprinting_protection_toggle_summary))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testFpProtectionToggle() {
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(true);
        doNothing().when(mDelegate).setFingerprintingProtection(anyBoolean());

        launchTrackingProtectionSettings();

        onView(withText(R.string.tracking_protection_fingerprinting_protection_toggle_summary))
                .check(matches(isDisplayed()));
        onView(allOf(withText(R.string.text_on), isDisplayed())).perform(click());

        verify(mDelegate).setFingerprintingProtection(false);
    }
}
