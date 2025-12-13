// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.browser_ui.accessibility.AccessibilitySettings.PREF_FORCE_ENABLE_ZOOM;
import static org.chromium.components.browser_ui.accessibility.AccessibilitySettings.PREF_IMAGE_DESCRIPTIONS;

import android.app.Instrumentation;
import android.content.Context;
import android.content.IntentFilter;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;

import androidx.preference.Preference;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Tests for the Accessibility Settings menu.
 *
 * <p>TODO(crbug.com/40214849): This tests the class in //components/browser_ui, but we don't have a
 * good way of testing with native code there.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Features.DisableFeatures({
    ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2,
    ContentFeatureList.SMART_ZOOM
})
@Batch(Batch.PER_CLASS)
public class AccessibilitySettingsTest {
    private AccessibilitySettings mAccessibilitySettings;

    @Rule
    public BlankUiTestActivitySettingsTestRule mSettingsActivityTestRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private BrowserContextHandle mContextHandleMock;

    @Mock private AccessibilitySettingsDelegate mDelegate;
    @Mock private AccessibilitySettingsDelegate.IntegerPreferenceDelegate mIntegerPrefMock;
    @Mock private AccessibilitySettingsDelegate.BooleanPreferenceDelegate mBoolPrefMock;
    @Mock private SettingsNavigation mSettingsNavigationMock;

    @Mock private HostZoomMapImpl.Natives mHostZoomMapBridgeMock;

    @Rule // initialize mocks
    public MockitoRule rule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapBridgeMock);

        when(mDelegate.getBrowserContextHandle()).thenReturn(mContextHandleMock);
        when(mDelegate.getForceEnableZoomAccessibilityDelegate()).thenReturn(mBoolPrefMock);
        when(mDelegate.getReaderAccessibilityDelegate()).thenReturn(mBoolPrefMock);
        when(mDelegate.getTouchpadOverscrollHistoryNavigationAccessibilityDelegate())
                .thenReturn(mBoolPrefMock);
        when(mDelegate.getTextSizeContrastAccessibilityDelegate()).thenReturn(mIntegerPrefMock);
        when(mDelegate.getSiteSettingsNavigation()).thenReturn(mSettingsNavigationMock);

        // Enable screen reader to display all settings options.
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsKnownScreenReaderEnabledForTesting(true));
        when(mDelegate.shouldShowImageDescriptionsSetting()).thenReturn(true);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsKnownScreenReaderEnabledForTesting(false));
        when(mDelegate.shouldShowImageDescriptionsSetting()).thenReturn(false);
    }

    // Generic AccessibilitySettings tests (no feature flag dependency).

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testForceEnableZoom() {
        launchPreferenceUI();
        ChromeSwitchPreference forceEnableZoomPref =
                (ChromeSwitchPreference)
                        mAccessibilitySettings.findPreference(PREF_FORCE_ENABLE_ZOOM);
        Assert.assertNotNull(forceEnableZoomPref);
        Assert.assertNotNull(forceEnableZoomPref.getOnPreferenceChangeListener());

        // The delegate has been called to fetch value when creating the page. Clear the invocations
        // so we can verify the correct number of invocations on user click.
        clearInvocations(mDelegate);

        // First scroll to the Force Enable Zoom preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(R.string.force_enable_zoom_title))));
        onView(withText(R.string.force_enable_zoom_title)).perform(click());
        Assert.assertTrue(
                "Force enable zoom option was not toggled", forceEnableZoomPref.isChecked());

        // On user click, we should fetch the setting delegate and set the value once.
        verify(mDelegate).getForceEnableZoomAccessibilityDelegate();
        verify(mBoolPrefMock).setValue(true);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testCaptionPreferences() {
        launchPreferenceUI();
        Preference captionsPref =
                mAccessibilitySettings.findPreference(AccessibilitySettings.PREF_CAPTIONS);
        Assert.assertNotNull(captionsPref);
        Assert.assertNotNull(captionsPref.getOnPreferenceClickListener());

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                new IntentFilter(Settings.ACTION_CAPTIONING_SETTINGS), null, false);

        // First scroll to the Captions preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(R.string.accessibility_captions_title))));
        onView(withText(R.string.accessibility_captions_title)).perform(click());
        monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testImageDescriptionsPreferences_Enabled() {
        launchPreferenceUI();
        Preference imageDescriptionsPref =
                mAccessibilitySettings.findPreference(PREF_IMAGE_DESCRIPTIONS);

        Assert.assertNotNull(imageDescriptionsPref);
        Assert.assertTrue(
                "Image Descriptions option should be visible", imageDescriptionsPref.isVisible());

        // First scroll to the Image Descriptions preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText("Image descriptions"))));
        onView(withText("Image descriptions")).perform(click());

        CriteriaHelper.pollInstrumentationThread(
                () ->
                        onView(withText("Only on Wi-Fi"))
                                .check(
                                        (v, e) ->
                                                Assert.assertEquals(
                                                        "Clicking image descriptions should open"
                                                                + " subpage",
                                                        View.VISIBLE,
                                                        v.getVisibility())));
    }

    // Tests related to Page Zoom Enhancements (fast-follow) feature.

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_savedZoomLevelsPreference() {
        launchPreferenceUI();
        Preference zoomInfoPref =
                mAccessibilitySettings.findPreference(AccessibilitySettings.PREF_ZOOM_INFO);
        Assert.assertNotNull(zoomInfoPref);
        Assert.assertNotNull(zoomInfoPref.getOnPreferenceClickListener());

        // First scroll to the "Saved zoom levels" preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(R.string.zoom_info_preference_title))));
        onView(withText(R.string.zoom_info_preference_title)).perform(click());

        verify(mSettingsNavigationMock)
                .startSettings(any(Context.class), any(), any(Bundle.class), eq(true));
    }

    // Tests related to Page Zoom V2 feature (OS-level adjustment experiments).

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2})
    public void testPageZoomPreference_osLevelAdjustmentPreference_visibleWhenEnabled() {
        launchPreferenceUI();
        ChromeSwitchPreference osLevelAdjustmentPref =
                (ChromeSwitchPreference)
                        mAccessibilitySettings.findPreference(
                                AccessibilitySettings.PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);
        Assert.assertNotNull(osLevelAdjustmentPref);
        Assert.assertNotNull(osLevelAdjustmentPref.getOnPreferenceChangeListener());

        // Current state of the preference (on/off).
        boolean initialSettingState = osLevelAdjustmentPref.isChecked();

        // First scroll to the "Match Android font size" preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(
                                        withText(R.string.page_zoom_include_os_adjustment_title))));
        onView(withText(R.string.page_zoom_include_os_adjustment_title)).perform(click());

        Assert.assertTrue(
                "OS-Level adjustment setting did not change on click",
                initialSettingState != osLevelAdjustmentPref.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @DisableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2})
    public void testPageZoomPreference_osLevelAdjustmentPreference_hiddenWhenDisabled() {
        launchPreferenceUI();
        Preference osLevelAdjustmentPref =
                mAccessibilitySettings.findPreference(
                        AccessibilitySettings.PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);
        Assert.assertNotNull(osLevelAdjustmentPref);
        Assert.assertFalse(
                "OS-Level adjustment settings should not be visible when disabled",
                osLevelAdjustmentPref.isVisible());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testReaderModePreferenceChange() {
        launchPreferenceUI();
        ChromeSwitchPreference readerModePref =
                (ChromeSwitchPreference)
                        mAccessibilitySettings.findPreference(
                                AccessibilitySettings.PREF_READER_FOR_ACCESSIBILITY);
        assertTrue(readerModePref.isVisible());
        boolean initialValue = readerModePref.isChecked();

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "DomDistiller.Android.ReaderModeEnabledInAccessibilitySettings",
                                !initialValue)
                        .build();
        readerModePref.callChangeListener(!initialValue);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.ANDROID_CARET_BROWSING})
    public void testCaretFeatureToggle() {
        DeviceInput.setSupportsKeyboardForTesting(true);
        launchPreferenceUI();
        ChromeSwitchPreference caretBrowsingPref =
                (ChromeSwitchPreference)
                        mAccessibilitySettings.findPreference(
                                AccessibilitySettings.PREF_CARET_BROWSING);
        assertTrue("Caret browsing toggle should be visible", caretBrowsingPref.isVisible());
        boolean initialValue = caretBrowsingPref.isChecked();

        // The delegate has been called to fetch value when creating the page. Clear the invocations
        // so we can verify the correct number of invocations on user click.
        clearInvocations(mDelegate);

        // First scroll to the caret browsing pref
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(R.string.caret_browsing_title))));
        onView(withText(R.string.caret_browsing_title)).perform(click());

        assertTrue(
                "Caret option setting was not toggled",
                initialValue != caretBrowsingPref.isChecked());

        // Verify that we did set the feature value on the delegate
        verify(mDelegate).setCaretBrowsingEnabled(any(Boolean.class));
    }

    // Helper methods.

    private static final BaseMatcher<View> sDisabled =
            new BaseMatcher<>() {
                @Override
                public boolean matches(Object o) {
                    return !((ChromeImageButton) o).isEnabled();
                }

                @Override
                public void describeTo(Description description) {
                    description.appendText("View was enabled, but should have been disabled.");
                }
            };

    private void launchPreferenceUI() {
        mSettingsActivityTestRule.launchPreference(
                AccessibilitySettings.class,
                null,
                (fragment) -> {
                    ((AccessibilitySettings) fragment).setDelegate(mDelegate);
                });
        mAccessibilitySettings =
                (AccessibilitySettings) mSettingsActivityTestRule.getPreferenceFragment();
    }
}
