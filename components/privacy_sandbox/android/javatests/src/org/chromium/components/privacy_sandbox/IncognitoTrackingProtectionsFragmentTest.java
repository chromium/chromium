// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isNotEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
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
    private static final int BLOCK_3PCS_TOGGLE_LABEL =
            R.string.incognito_tracking_protections_block_3pcs_toggle_label;
    private static final int IP_PROTECTION_TOGGLE_LABEL =
            R.string.incognito_tracking_protections_ip_protection_toggle_label;
    private static final int IP_PROTECTION_TOGGLE_SUBLABEL_OFF =
            R.string.incognito_tracking_protections_ip_protection_toggle_sublabel_off;
    private static final int IP_PROTECTION_TOGGLE_SUBLABEL_ON =
            R.string.incognito_tracking_protections_ip_protection_toggle_sublabel_on;
    private static final int FINGERPRINTING_PROTECTION_TOGGLE_LABEL =
            R.string.incognito_tracking_protections_fingerprinting_protection_toggle_label;
    private static final int FINGERPRINTING_PROTECTION_TOGGLE_SUBLABEL_OFF =
            R.string.incognito_tracking_protections_fingerprinting_protection_toggle_sublabel_off;
    private static final int FINGERPRINTING_PROTECTION_TOGGLE_SUBLABEL_ON =
            R.string.incognito_tracking_protections_fingerprinting_protection_toggle_sublabel_on;

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private TrackingProtectionDelegate mDelegate;

    private IncognitoTrackingProtectionsFragment mFragment;

    @BeforeClass
    public static void setUpSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
    }

    private void launchIncognitoTrackingProtectionsSettings() {
        mSettingsRule.launchPreference(
                IncognitoTrackingProtectionsFragment.class,
                null,
                (fragment) ->
                        ((IncognitoTrackingProtectionsFragment) fragment)
                                .setTrackingProtectionDelegate(mDelegate));
        mFragment = (IncognitoTrackingProtectionsFragment) mSettingsRule.getPreferenceFragment();
    }

    private Matcher<View> getBlock3pcsToggleMatcher() {
        return allOf(
                withId(R.id.switchWidget),
                withParent(withParent(hasDescendant(withText(BLOCK_3PCS_TOGGLE_LABEL)))));
    }

    private void checkTitleDescriptionAndBlock3pcsToggleAreShown() {
        assertEquals(
                mFragment
                        .getContext()
                        .getString(R.string.incognito_tracking_protections_page_title),
                mFragment.getPageTitle().get());

        onView(withText(R.string.incognito_tracking_protections_page_description))
                .check(matches(isDisplayed()));
        onView(getBlock3pcsToggleMatcher())
                .check(matches(allOf(isDisplayed(), isChecked(), isNotEnabled())));
    }

    @Test
    @SmallTest
    public void showIncognitoTrackingProtectionsWhenFingerprintingProtectionUxEnabled() {
        when(mDelegate.isFingerprintingProtectionUxEnabled()).thenReturn(true);
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(false);

        launchIncognitoTrackingProtectionsSettings();

        checkTitleDescriptionAndBlock3pcsToggleAreShown();
        onView(
                        allOf(
                                withText(FINGERPRINTING_PROTECTION_TOGGLE_LABEL),
                                hasSibling(
                                        withText(FINGERPRINTING_PROTECTION_TOGGLE_SUBLABEL_OFF))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void showFingerprintingProtectionOnSublabelWhenEnabled() {
        when(mDelegate.isFingerprintingProtectionUxEnabled()).thenReturn(true);
        when(mDelegate.isFingerprintingProtectionEnabled()).thenReturn(true);

        launchIncognitoTrackingProtectionsSettings();

        checkTitleDescriptionAndBlock3pcsToggleAreShown();
        onView(
                        allOf(
                                withText(FINGERPRINTING_PROTECTION_TOGGLE_LABEL),
                                hasSibling(withText(FINGERPRINTING_PROTECTION_TOGGLE_SUBLABEL_ON))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void showIncognitoTrackingProtectionsWhenIpProtectionEnabled() {
        when(mDelegate.isIpProtectionUxEnabled()).thenReturn(true);
        when(mDelegate.isIpProtectionEnabled()).thenReturn(false);

        launchIncognitoTrackingProtectionsSettings();

        checkTitleDescriptionAndBlock3pcsToggleAreShown();
        onView(
                        allOf(
                                withText(IP_PROTECTION_TOGGLE_LABEL),
                                hasSibling(withText(IP_PROTECTION_TOGGLE_SUBLABEL_OFF))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void showIpProtectionOnSublabelWhenEnabled() {
        when(mDelegate.isIpProtectionUxEnabled()).thenReturn(true);
        when(mDelegate.isIpProtectionEnabled()).thenReturn(true);

        launchIncognitoTrackingProtectionsSettings();

        checkTitleDescriptionAndBlock3pcsToggleAreShown();
        onView(
                        allOf(
                                withText(IP_PROTECTION_TOGGLE_LABEL),
                                hasSibling(withText(IP_PROTECTION_TOGGLE_SUBLABEL_ON))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void showIpProtectionOffSublabelWhenIpProtectionDisabledForEnterprise() {
        when(mDelegate.isIpProtectionUxEnabled()).thenReturn(true);
        when(mDelegate.isIpProtectionDisabledForEnterprise()).thenReturn(true);

        launchIncognitoTrackingProtectionsSettings();

        checkTitleDescriptionAndBlock3pcsToggleAreShown();
        onView(
                        allOf(
                                withText(IP_PROTECTION_TOGGLE_LABEL),
                                hasSibling(withText(IP_PROTECTION_TOGGLE_SUBLABEL_OFF))))
                .check(matches(isDisplayed()));
    }
}
