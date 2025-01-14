// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
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

    @Mock private WebsitePreferenceBridge.Natives mBridgeMock;

    @Mock private BrowserContextHandle mContextHandleMock;

    @Mock private TrackingProtectionDelegate mDelegate;

    @Mock private SiteSettingsDelegate mSiteSettingsDelegate;

    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    private TrackingProtectionSettings mFragment;

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mBridgeMock);

        when(mDelegate.getBrowserContext()).thenReturn(mContextHandleMock);
        when(mDelegate.getSiteSettingsDelegate(any(Context.class)))
                .thenReturn(mSiteSettingsDelegate);
    }

    private void launchTrackingProtectionSettings() {
        mSettingsRule.launchPreference(
                TrackingProtectionSettings.class,
                null,
                (fragment) -> {
                    ((TrackingProtectionSettings) fragment)
                            .setTrackingProtectionDelegate(mDelegate);
                    ((TrackingProtectionSettings) fragment)
                            .setCustomTabLauncher(mCustomTabLauncher);
                });
        mFragment = (TrackingProtectionSettings) mSettingsRule.getPreferenceFragment();
    }

    @Test
    @SmallTest
    public void launchTrackingProtectionPage() {
        when(mDelegate.isBlockAll3pcEnabled()).thenReturn(true);
        when(mDelegate.isDoNotTrackEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        onView(withText(R.string.privacy_sandbox_tracking_protection_description))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void launchWithIpAndFpProtection_extraContentDisplayed() {
        when(mDelegate.isBlockAll3pcEnabled()).thenReturn(true);
        when(mDelegate.isDoNotTrackEnabled()).thenReturn(true);
        when(mDelegate.shouldDisplayIpProtection()).thenReturn(true);
        when(mDelegate.shouldDisplayFingerprintingProtection()).thenReturn(true);

        launchTrackingProtectionSettings();

        Context context = ApplicationProvider.getApplicationContext();
        String fp_protection =
                context.getString(R.string.tracking_protection_fingerprinting_protection_learn_more)
                        .replaceAll("<link>|</link>", "");
        String ip_protection =
                context.getString(R.string.tracking_protection_ip_protection_learn_more)
                        .replaceAll("<link>|</link>", "");
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(withText(fp_protection))));
        onView(withText(ip_protection)).check(matches(isDisplayed()));
        onView(withText(fp_protection)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void changeToggleValues_propagatedToBackend() {
        when(mDelegate.isBlockAll3pcEnabled()).thenReturn(true);
        when(mDelegate.isDoNotTrackEnabled()).thenReturn(true);
        when(mDelegate.shouldDisplayIpProtection()).thenReturn(true);
        when(mDelegate.shouldDisplayFingerprintingProtection()).thenReturn(true);
        when(mDelegate.isIpProtectionEnabled()).thenReturn(false);
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(false);

        launchTrackingProtectionSettings();

        onView(withText(R.string.tracking_protection_block_cookies_toggle_title)).perform(click());
        verify(mDelegate).setBlockAll3pc(/* enabled= */ Mockito.eq(false));

        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(
                                        withText(
                                                R.string
                                                        .tracking_protection_fingerprinting_protection_title))));

        onView(withText(R.string.tracking_protection_ip_protection_toggle_title)).perform(click());
        verify(mDelegate).setIpProtection(/* enabled= */ Mockito.eq(true));
        onView(withText(R.string.tracking_protection_fingerprinting_protection_title))
                .perform(click());
        verify(mDelegate).setFingerprintingProtection(/* enabled= */ Mockito.eq(true));
    }

    @Test
    @SmallTest
    public void clickOnLearnMore_cctIsOpened() {
        when(mDelegate.isBlockAll3pcEnabled()).thenReturn(true);
        when(mDelegate.isDoNotTrackEnabled()).thenReturn(true);

        launchTrackingProtectionSettings();

        Context context = ApplicationProvider.getApplicationContext();
        String tp_learn_more =
                context.getString(
                                R.string.privacy_sandbox_tracking_protection_bullet_two_description)
                        .replaceAll("<link>|</link>", "");
        onView(withText(tp_learn_more)).perform(clickOnClickableSpan(/* spanIndex= */ 0));
        verify(mCustomTabLauncher)
                .openUrlInCct(
                        /* context= */ Mockito.any(),
                        /* url= */ Mockito.eq(TrackingProtectionSettings.LEARN_MORE_URL));
    }
}
