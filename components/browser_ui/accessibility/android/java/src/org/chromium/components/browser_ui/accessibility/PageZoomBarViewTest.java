// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import com.google.android.material.slider.Slider;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.ContentFeatureMapJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;
import java.util.List;

/** Unit tests for the PageZoom view and view binder. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@DisableFeatures({
    ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2,
    ContentFeatureList.SMART_ZOOM,
    ContentFeatures.ANDROID_DESKTOP_ZOOM_SCALING
})
@Batch(Batch.PER_CLASS)
public class PageZoomBarViewTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("useSlider_false"),
                    new ParameterSet().value(true).name("useSlider_true"));

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static ViewGroup sContentView;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HostZoomMapImpl.Natives mHostZoomMapJniMock;
    @Mock private ContentFeatureMap.Natives mContentFeatureListMapMock;
    @Mock private PageZoomMetrics.Natives mPageZoomMetricsJniMock;
    @Mock private BrowserContextHandle mBrowserContextHandle;
    @Mock private MockWebContents mWebContents;

    private PageZoomBarCoordinator mCoordinator;
    private PageZoomBarCoordinatorDelegate mDelegate;
    private PageZoomManagerDelegate mPageZoomManagerDelegate;
    private View mPageZoomView;
    private final boolean mUseSlider;
    private Slider mSlider;
    private SeekBar mSeekBar;

    public PageZoomBarViewTest(boolean useSlider) {
        mUseSlider = useSlider;
    }

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                    sContentView = new FrameLayout(sActivity);
                    sActivity.setContentView(sContentView);
                });
    }

    @AfterClass
    public static void tearDownSuite() {
        sActivity = null;
        sContentView = null;
    }

    @Before
    public void setUp() throws Exception {
        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapJniMock);
        ContentFeatureMapJni.setInstanceForTesting(mContentFeatureListMapMock);
        PageZoomMetricsJni.setInstanceForTesting(mPageZoomMetricsJniMock);
        when(mHostZoomMapJniMock.getDefaultZoomLevel(any())).thenReturn(0.0);
        when(mHostZoomMapJniMock.getZoomLevel(any())).thenReturn(0.0);

        mDelegate =
                new PageZoomBarCoordinatorDelegate() {
                    @Override
                    public View getZoomControlView() {
                        return mPageZoomView;
                    }
                };

        mPageZoomManagerDelegate =
                new PageZoomManagerDelegate() {
                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }

                    @Override
                    public BrowserContextHandle getBrowserContextHandle() {
                        return mBrowserContextHandle;
                    }

                    @Override
                    public void removeZoomEventsObserver(ZoomEventsObserver observer) {}

                    @Override
                    public void addZoomEventsObserver(ZoomEventsObserver observer) {}

                    @Override
                    public void enterImmersiveMode() {}

                    @Override
                    public boolean isCurrentTabNull() {
                        return false;
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();
                    mPageZoomView =
                            LayoutInflater.from(sActivity)
                                    .inflate(R.layout.page_zoom_view, sContentView, false);
                    sContentView.addView(mPageZoomView);

                    mCoordinator =
                            new PageZoomBarCoordinator(
                                    mDelegate,
                                    new PageZoomManager(mPageZoomManagerDelegate),
                                    mUseSlider);
                    mSlider = mPageZoomView.findViewById(R.id.page_zoom_slider);
                    mSeekBar = mPageZoomView.findViewById(R.id.page_zoom_slider_legacy);
                    mCoordinator.show(mWebContents);
                });
    }

    private int getBarValue() {
        if (mUseSlider) {
            return (int) mSlider.getValue();
        }
        return mSeekBar.getProgress();
    }

    private View getVisibleBar() {
        if (mUseSlider) {
            return mSlider;
        }
        return mSeekBar;
    }

    // Test cases.

    @Test
    @SmallTest
    public void testSetup() {
        // Verify that the view has been drawn.
        assertEquals(View.VISIBLE, mPageZoomView.getVisibility());
        assertEquals(
                View.VISIBLE,
                mPageZoomView.findViewById(R.id.page_zoom_current_zoom_level).getVisibility());
        assertEquals(
                View.VISIBLE,
                mPageZoomView.findViewById(R.id.page_zoom_view_container).getVisibility());
        assertEquals(
                View.VISIBLE,
                mPageZoomView.findViewById(R.id.page_zoom_decrease_zoom_button).getVisibility());
        assertEquals(View.VISIBLE, getVisibleBar().getVisibility());
        assertEquals(
                View.VISIBLE,
                mPageZoomView.findViewById(R.id.page_zoom_increase_zoom_button).getVisibility());
        assertEquals(
                View.VISIBLE,
                mPageZoomView.findViewById(R.id.page_zoom_reset_divider).getVisibility());
        assertEquals(
                View.VISIBLE,
                mPageZoomView.findViewById(R.id.page_zoom_reset_zoom_button).getVisibility());
    }

    @Test
    @SmallTest
    public void testContent() {
        // Verify that all content is correct.
        assertFalse(
                mPageZoomView
                        .findViewById(R.id.page_zoom_view_container)
                        .isImportantForAccessibility());
        assertTextContent("100");
        assertTrue(
                mPageZoomView
                        .findViewById(R.id.page_zoom_decrease_zoom_button)
                        .getContentDescription()
                        .equals("Decrease zoom"));
        assertTrue(
                mPageZoomView
                        .findViewById(R.id.page_zoom_increase_zoom_button)
                        .getContentDescription()
                        .equals("Increase zoom"));
    }

    @Test
    @SmallTest
    public void testDecreaseButton() {
        assertEquals(50, getBarValue());
        assertViewState("100", true, true);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                PageZoomUma.PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_CHANGED_HISTOGRAM,
                                true)
                        .expectIntRecord(
                                PageZoomUma.PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_VALUE_HISTOGRAM,
                                90)
                        .build();

        onView(withId(R.id.page_zoom_decrease_zoom_button)).perform(click());
        assertEquals(40, getBarValue());
        assertViewState("90", true, true);

        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.hide());

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testIncreaseButton() {
        assertEquals(50, getBarValue());
        assertViewState("100", true, true);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                PageZoomUma.PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_CHANGED_HISTOGRAM,
                                true)
                        .expectIntRecord(
                                PageZoomUma.PAGE_ZOOM_APP_MENU_SLIDER_ZOOM_LEVEL_VALUE_HISTOGRAM,
                                110)
                        .build();

        onView(withId(R.id.page_zoom_increase_zoom_button)).perform(click());
        assertEquals(60, getBarValue());
        assertViewState("110", true, true);

        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.hide());

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testResetButton() {
        assertEquals(50, getBarValue());
        assertViewState("100", true, true);

        onView(withId(R.id.page_zoom_increase_zoom_button)).perform(click());
        assertViewState("110", true, true);

        // Reset the zoom by directly calling the coordinator's testing method.
        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.resetZoomForTesting());

        // Verify the view has updated back to 100%.
        assertEquals(50, getBarValue());
        assertViewState("100", true, true);
    }

    // Helper methods for simple checks.

    private void assertViewState(
            String expectedText,
            boolean expectedDecreaseButtonEnabled,
            boolean expectedIncreaseButtonEnabled) {
        assertTextContent(expectedText);
        assertButtonStates(expectedDecreaseButtonEnabled, expectedIncreaseButtonEnabled);
    }

    private void assertTextContent(String expected) {
        assertTrue(
                ((TextView) mPageZoomView.findViewById(R.id.page_zoom_current_zoom_level))
                        .getText()
                        .equals(expected + " %"));
        assertTrue(
                mPageZoomView
                        .findViewById(R.id.page_zoom_current_zoom_level)
                        .getContentDescription()
                        .equals("Current zoom is " + expected + " %"));
    }

    private void assertButtonStates(boolean decreaseExpected, boolean increaseExpected) {
        assertEquals(
                decreaseExpected,
                mPageZoomView.findViewById(R.id.page_zoom_decrease_zoom_button).isEnabled());
        assertEquals(
                increaseExpected,
                mPageZoomView.findViewById(R.id.page_zoom_increase_zoom_button).isEnabled());
    }
}
