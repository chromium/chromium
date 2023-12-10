// Copyright 2023 The Chromium Authors
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
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Tests for TrackingProtectionSettings. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TrackingProtectionSettingsTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private WebsitePreferenceBridge.Natives mBridgeMock;

    @Mock private BrowserContextHandle mContextHandleMock;

    @Mock private TrackingProtectionDelegate mDelegate;

    private TrackingProtectionSettings mFragment;

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mBridgeMock);

        when(mDelegate.getBrowserContext()).thenReturn(mContextHandleMock);
    }

    private void launchTrackingProtectionSettings() {
        mSettingsRule.launchPreference(
                TrackingProtectionSettings.class,
                null,
                (fragment) -> {
                    ((TrackingProtectionSettings) fragment)
                            .setTrackingProtectionDelegate(mDelegate);
                });
        mFragment = (TrackingProtectionSettings) mSettingsRule.getPreferenceFragment();
    }

    @Test
    @SmallTest
    public void testShowTrackingProtectionUi() {
        when(mDelegate.isBlockAll3PCDEnabled()).thenReturn(true);
        when(mDelegate.isDoNotTrackEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        onView(withText(R.string.privacy_sandbox_tracking_protection_description))
                .check(matches(isDisplayed()));
    }
}
