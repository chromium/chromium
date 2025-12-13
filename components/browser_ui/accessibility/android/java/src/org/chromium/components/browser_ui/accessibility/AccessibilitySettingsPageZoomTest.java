// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.SmallTest;

import org.hamcrest.BaseMatcher;
import org.hamcrest.CoreMatchers;
import org.hamcrest.Description;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.Arrays;
import java.util.List;

/** Tests for the Accessibility Settings menu's Seekbar. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Features.DisableFeatures({
    ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2,
    ContentFeatureList.SMART_ZOOM
})
public class AccessibilitySettingsPageZoomTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("useSlider_false"),
                    new ParameterSet().value(true).name("useSlider_true"));

    private final boolean mUseSlider;
    private AccessibilitySettings mAccessibilitySettings;
    private PageZoomPreference mPageZoomPref;

    public AccessibilitySettingsPageZoomTest(boolean useSlider) {
        mUseSlider = useSlider;
    }

    @Rule
    public BlankUiTestActivitySettingsTestRule mSettingsActivityTestRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private BrowserContextHandle mContextHandleMock;

    @Mock private AccessibilitySettingsDelegate mDelegate;
    @Mock private AccessibilitySettingsDelegate.IntegerPreferenceDelegate mIntegerPrefMock;
    @Mock private AccessibilitySettingsDelegate.BooleanPreferenceDelegate mBoolPrefMock;
    @Mock private SettingsNavigation mSettingsNavigationMock;

    @Mock private HostZoomMapImpl.Natives mHostZoomMapBridgeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapBridgeMock);

        when(mDelegate.getBrowserContextHandle()).thenReturn(mContextHandleMock);
        when(mDelegate.getForceEnableZoomAccessibilityDelegate()).thenReturn(mBoolPrefMock);
        when(mDelegate.getReaderAccessibilityDelegate()).thenReturn(mBoolPrefMock);
        when(mDelegate.getTouchpadOverscrollHistoryNavigationAccessibilityDelegate())
                .thenReturn(mBoolPrefMock);
        when(mDelegate.getTextSizeContrastAccessibilityDelegate()).thenReturn(mIntegerPrefMock);
        when(mDelegate.getSiteSettingsNavigation()).thenReturn(mSettingsNavigationMock);
        when(mDelegate.shouldUseSlider()).thenReturn(mUseSlider);

        // Enable screen reader to display all settings options.
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsKnownScreenReaderEnabledForTesting(true));
        when(mDelegate.shouldShowImageDescriptionsSetting()).thenReturn(true);

        mSettingsActivityTestRule.launchPreference(
                AccessibilitySettings.class,
                null,
                (fragment) -> ((AccessibilitySettings) fragment).setDelegate(mDelegate));
        mAccessibilitySettings =
                (AccessibilitySettings) mSettingsActivityTestRule.getPreferenceFragment();

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsKnownScreenReaderEnabledForTesting(false));
        when(mDelegate.shouldShowImageDescriptionsSetting()).thenReturn(false);
    }

    // Tests related to Page Zoom feature.

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_decreaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getCurrentZoomValue();
        onView(withId(R.id.page_zoom_decrease_zoom_button)).perform(click());
        Assert.assertTrue(startingVal > mPageZoomPref.getCurrentZoomValue());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_decreaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(() -> mPageZoomPref.setZoomValueForTesting(0));
        onView(withId(R.id.page_zoom_decrease_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_increaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getCurrentZoomValue();
        onView(withId(R.id.page_zoom_increase_zoom_button)).perform(click());
        Assert.assertTrue(startingVal < mPageZoomPref.getCurrentZoomValue());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_increaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPageZoomPref.setZoomValueForTesting(
                                PageZoomUtils.PAGE_ZOOM_MAXIMUM_BAR_VALUE));
        onView(withId(R.id.page_zoom_increase_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_zoomSliderUpdatesValue() {
        getPageZoomPref();
        int startingVal = mPageZoomPref.getCurrentZoomValue();
        onSliderView(R.id.page_zoom_slider, R.id.page_zoom_slider_legacy)
                .perform(ViewActions.swipeRight());
        Assert.assertNotEquals(startingVal, mPageZoomPref.getCurrentZoomValue());
    }

    // Tests related to the Smart Zoom feature.

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_smartZoom_hiddenWhenDisabled() {
        getPageZoomPref();
        onView(withId(R.id.text_size_contrast_section))
                .check(matches(CoreMatchers.not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_visibleWhenEnabled() {
        getPageZoomPref();
        onView(withId(R.id.text_size_contrast_section)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_decreaseButtonUpdatesValue() {
        getPageZoomPref();

        ThreadUtils.runOnUiThreadBlocking(() -> mPageZoomPref.setTextContrastValueForTesting(20));
        int startingVal = mPageZoomPref.getCurrentContrastValue();
        onView(withId(R.id.text_size_contrast_decrease_zoom_button)).perform(click());
        Assert.assertTrue(startingVal > mPageZoomPref.getCurrentContrastValue());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_decreaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(() -> mPageZoomPref.setTextContrastValueForTesting(0));
        onView(withId(R.id.text_size_contrast_decrease_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_increaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getCurrentContrastValue();
        onView(withId(R.id.text_size_contrast_increase_zoom_button)).perform(click());
        Assert.assertTrue(startingVal < mPageZoomPref.getCurrentContrastValue());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_increaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPageZoomPref.setTextContrastValueForTesting(
                                PageZoomUtils.TEXT_SIZE_CONTRAST_MAX_LEVEL));
        onView(withId(R.id.text_size_contrast_increase_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_zoomSliderUpdatesValue() {
        getPageZoomPref();
        int startingVal = mPageZoomPref.getCurrentContrastValue();
        onSliderView(R.id.text_size_contrast_slider, R.id.text_size_contrast_slider_legacy)
                .perform(ViewActions.swipeRight());
        Assert.assertNotEquals(startingVal, mPageZoomPref.getCurrentContrastValue());
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

    private void getPageZoomPref() {
        mPageZoomPref =
                mAccessibilitySettings.findPreference(
                        AccessibilitySettings.PREF_PAGE_ZOOM_DEFAULT_ZOOM);
        Assert.assertNotNull(mPageZoomPref);
        Assert.assertTrue("Page Zoom pref should be visible.", mPageZoomPref.isVisible());
    }

    private ViewInteraction onSliderView(int sliderId, int legacySliderId) {
        return onView(withId(mUseSlider ? sliderId : legacySliderId));
    }
}
