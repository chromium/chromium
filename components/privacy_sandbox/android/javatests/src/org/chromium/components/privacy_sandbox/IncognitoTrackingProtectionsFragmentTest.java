// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;

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

/** Tests for {@link IncognitoTrackingProtectionsFragment}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class IncognitoTrackingProtectionsFragmentTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private TrackingProtectionDelegate mDelegate;

    private IncognitoTrackingProtectionsFragment mFragment;

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
    }

    private void launchIncognitoTrackingProtectionsSettings() {
        mSettingsRule.launchPreference(IncognitoTrackingProtectionsFragment.class, null);
        mFragment = (IncognitoTrackingProtectionsFragment) mSettingsRule.getPreferenceFragment();
    }

    @Test
    @SmallTest
    public void showIncognitoTrackingProtectionsTitle() {
        launchIncognitoTrackingProtectionsSettings();

        onView(withText(R.string.incognito_tracking_protections_page_title))
                .check(matches(isDisplayed()));
        assertEquals(
                mFragment
                        .getContext()
                        .getString(R.string.incognito_tracking_protections_page_title),
                mFragment.getPageTitle().get());
    }
}
