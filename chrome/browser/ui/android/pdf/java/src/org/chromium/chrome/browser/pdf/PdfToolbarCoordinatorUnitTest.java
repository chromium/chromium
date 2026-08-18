// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.AdditionalMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfToolbarAction;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ChromePopupWindow;
import org.chromium.ui.widget.UiWidgetFactory;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.INLINE_PDF_V2, ChromeFeatureList.INLINE_PDF_V2_DOWNLOAD})
public class PdfToolbarCoordinatorUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private PdfToolbarActionsDelegate mDelegate;

    private Activity mActivity;
    private View mPdfPageView;
    private PdfToolbarCoordinator mPdfToolbarCoordinator;
    private AutoCloseable mCloseableMocks;
    private UiWidgetFactory mMockUiWidgetFactory;
    private ChromePopupWindow mSpyPopupWindow;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mMockUiWidgetFactory = mock(UiWidgetFactory.class);
        mSpyPopupWindow = spy(new ChromePopupWindow(mActivity));
        UiWidgetFactory.setInstance(mMockUiWidgetFactory);
        when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        PdfUtils.setInlinePdfV2EditEnabledForTesting(true);
        mPdfPageView = LayoutInflater.from(mActivity).inflate(R.layout.pdf_page, null);
        mPdfToolbarCoordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        mPdfToolbarCoordinator.onDocumentLoaded(100, "test_title.pdf");
        mPdfToolbarCoordinator.onViewportChanged(98, 1); // 0-indexed page 98

    }

    @After
    public void tearDown() throws Exception {
        mCloseableMocks.close();
        UiWidgetFactory.setInstance(null);
    }

    private void setToolbarWidth(int widthDp) {
        PdfToolbar toolbar = mPdfPageView.findViewById(R.id.pdf_toolbar);
        setToolbarWidth(toolbar, widthDp);
    }

    private void setToolbarWidth(PdfToolbar toolbar, int widthDp) {
        float density = mActivity.getResources().getDisplayMetrics().density;
        toolbar.layout(0, 0, (int) (widthDp * density), 56);
    }

    @Test
    public void testPageNumberEdit() {
        // Default current page is 99 (1-indexed), total is 100
        EditText currentPage = mPdfPageView.findViewById(R.id.current_page);

        // Request focus to enable editing
        assertTrue(currentPage.requestFocus());
        assertTrue(currentPage.isFocused());

        // Simulate typing valid page and submitting
        currentPage.setText("50");
        currentPage.onEditorAction(android.view.inputmethod.EditorInfo.IME_ACTION_GO);

        // Should navigate to 0-indexed page 49
        verify(mDelegate).navigateToPage(49);

        // Verify it loses focus
        assertFalse(currentPage.isFocused());
    }

    @Test
    public void testPageNumberEdit_invalid() {
        EditText currentPage = mPdfPageView.findViewById(R.id.current_page);

        // Out of bounds high
        assertTrue(currentPage.requestFocus());
        assertTrue(currentPage.isFocused());
        currentPage.setText("101");
        currentPage.onEditorAction(android.view.inputmethod.EditorInfo.IME_ACTION_GO);
        // Should NOT navigate to 100
        verify(mDelegate, org.mockito.Mockito.never()).navigateToPage(100);
        // Should revert to 99
        assertEquals("99", currentPage.getText().toString());
        assertFalse(currentPage.isFocused());

        // Out of bounds low
        assertTrue(currentPage.requestFocus());
        assertTrue(currentPage.isFocused());
        currentPage.setText("0");
        currentPage.onEditorAction(android.view.inputmethod.EditorInfo.IME_ACTION_GO);
        verify(mDelegate, org.mockito.Mockito.never()).navigateToPage(-1);
        // Should revert to 99
        assertEquals("99", currentPage.getText().toString());
        assertFalse(currentPage.isFocused());

        // Number overflow / excessively large input string
        assertTrue(currentPage.requestFocus());
        assertTrue(currentPage.isFocused());
        currentPage.setText("7868768761");
        currentPage.onEditorAction(android.view.inputmethod.EditorInfo.IME_ACTION_GO);
        // Should revert to 99
        assertEquals("99", currentPage.getText().toString());
        assertFalse(currentPage.isFocused());
    }

    @Test
    public void testViewportChanged() {
        mPdfToolbarCoordinator.onViewportChanged(5, 1);
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        TextView pageCountDivider = mPdfPageView.findViewById(R.id.page_count_divider);
        TextView pageCount = mPdfPageView.findViewById(R.id.page_count);
        TextView zoomValue = mPdfPageView.findViewById(R.id.zoom_value);
        // Current page is firstVisiblePage + 1
        assertEquals(
                "6 / 100",
                currentPage.getText().toString()
                        + pageCountDivider.getText().toString()
                        + pageCount.getText().toString());
        assertEquals("100%", zoomValue.getText().toString());
    }

    @Test
    public void testDefaultZoomNormalizedTo100Percent() {
        // Create a new coordinator and simulate initial document load with non-1.0 default zoom
        // (e.g. 0.6f)
        PdfToolbarCoordinator coordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        coordinator.onDocumentLoaded(50, "sample.pdf");
        coordinator.setDefaultZoomLevel(0.6f);
        coordinator.onViewportChanged(0, 0.6f);

        TextView zoomValue = mPdfPageView.findViewById(R.id.zoom_value);
        // Initial zoom of 0.6f should be normalized and displayed as 100%
        assertEquals("100%", zoomValue.getText().toString());
        assertEquals(0.6f, coordinator.getDefaultZoomLevel(), 0.001f);

        // Zooming to 1.2f (2x default zoom) should read as 200%
        coordinator.onViewportChanged(0, 1.2f);
        assertEquals("200%", zoomValue.getText().toString());

        // Zooming to 0.3f (0.5x default zoom) should read as 50%
        coordinator.onViewportChanged(0, 0.3f);
        assertEquals("50%", zoomValue.getText().toString());
    }

    @Test
    public void testZoomButtonsScaleRelativeDefaultZoom() {
        PdfToolbarCoordinator coordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        coordinator.onDocumentLoaded(50, "sample.pdf");
        coordinator.setDefaultZoomLevel(0.6f);
        coordinator.onViewportChanged(0, 0.6f); // Default zoom = 0.6f (100% display)

        View zoomIncreaseButton = mPdfPageView.findViewById(R.id.zoom_increase_button);
        zoomIncreaseButton.performClick();
        // Next display step after 1.0f is 1.1f -> engine zoom = 1.1f * 0.6f = 0.66f
        verify(mDelegate).changeZoomLevel(AdditionalMatchers.eq(1.1f * 0.6f, 0.001f));

        View zoomDecreaseButton = mPdfPageView.findViewById(R.id.zoom_decrease_button);
        zoomDecreaseButton.performClick();
        // Previous display step before 1.0f is 0.9f -> engine zoom = 0.9f * 0.6f = 0.54f
        verify(mDelegate).changeZoomLevel(AdditionalMatchers.eq(0.9f * 0.6f, 0.001f));
    }

    @Test
    public void testDocumentReloadResetsDefaultZoom() {
        PdfToolbarCoordinator coordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        coordinator.onDocumentLoaded(50, "first.pdf");
        coordinator.setDefaultZoomLevel(0.6f);
        coordinator.onViewportChanged(0, 0.6f);
        assertEquals(0.6f, coordinator.getDefaultZoomLevel(), 0.001f);

        // Reload a new document with different initial zoom (e.g. 1.5f)
        coordinator.onDocumentLoaded(10, "second.pdf");
        assertEquals(-1.0f, coordinator.getDefaultZoomLevel(), 0.001f);

        coordinator.setDefaultZoomLevel(1.5f);
        coordinator.onViewportChanged(0, 1.5f);
        assertEquals(1.5f, coordinator.getDefaultZoomLevel(), 0.001f);
        TextView zoomValue = mPdfPageView.findViewById(R.id.zoom_value);
        assertEquals("100%", zoomValue.getText().toString());
    }

    @Test
    public void testOnViewportChanged_indexing() {
        // Input is 0-indexed (page 0), output should be 1-indexed ("1")
        mPdfToolbarCoordinator.onViewportChanged(0, 1);
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        assertEquals("1", currentPage.getText().toString());

        // Input is 0-indexed (page 5), output should be 1-indexed ("6")
        mPdfToolbarCoordinator.onViewportChanged(5, 1);
        assertEquals("6", currentPage.getText().toString());
    }

    @Test
    public void testViewInit() {
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        TextView pageCountDivider = mPdfPageView.findViewById(R.id.page_count_divider);
        TextView pageCount = mPdfPageView.findViewById(R.id.page_count);
        assertEquals(
                "99 / 100",
                currentPage.getText().toString()
                        + pageCountDivider.getText().toString()
                        + pageCount.getText().toString());
        TextView zoomValue = mPdfPageView.findViewById(R.id.zoom_value);
        assertEquals("100%", zoomValue.getText().toString());
    }

    // Regression test: onViewportChanged with a zoom value just below 5.0
    // formats as "500%" via "%.0f%%", which parses back to exactly 5.0f.  When the user then
    // clicks zoom-in, getNextEngineZoomLevel(5.0f, true) used to throw IndexOutOfBoundsException
    // because the while-loop advanced index to mZoomLevels.size().
    @Test
    public void testZoomIncrease_atMaxZoom_doesNotCrash() {
        mPdfToolbarCoordinator.onViewportChanged(0, 4.999f);
        View zoomIncreaseButton = mPdfPageView.findViewById(R.id.zoom_increase_button);
        // Should not throw and should clamp to the maximum zoom level (5.0f).
        zoomIncreaseButton.performClick();
        verify(mDelegate).changeZoomLevel(5.0f);
    }

    @Test
    public void testZoomButtonsEnablement_atBoundaries() {
        View zoomIncreaseButton = mPdfPageView.findViewById(R.id.zoom_increase_button);
        View zoomDecreaseButton = mPdfPageView.findViewById(R.id.zoom_decrease_button);

        // At minimum zoom (0.25f): decrease is disabled, increase is enabled.
        mPdfToolbarCoordinator.onViewportChanged(0, 0.25f);
        assertFalse(zoomDecreaseButton.isEnabled());
        assertTrue(zoomIncreaseButton.isEnabled());

        // Within ZOOM_EPSILON above minimum zoom (e.g. 0.254f <= 0.255f): decrease is disabled.
        mPdfToolbarCoordinator.onViewportChanged(0, 0.254f);
        assertFalse(zoomDecreaseButton.isEnabled());
        assertTrue(zoomIncreaseButton.isEnabled());

        // Beyond ZOOM_EPSILON above minimum zoom (e.g. 0.256f > 0.255f): decrease is enabled.
        mPdfToolbarCoordinator.onViewportChanged(0, 0.256f);
        assertTrue(zoomDecreaseButton.isEnabled());
        assertTrue(zoomIncreaseButton.isEnabled());

        // At maximum zoom (5.0f): increase is disabled, decrease is enabled.
        mPdfToolbarCoordinator.onViewportChanged(0, 5.0f);
        assertTrue(zoomDecreaseButton.isEnabled());
        assertFalse(zoomIncreaseButton.isEnabled());

        // Within ZOOM_EPSILON below maximum zoom (e.g. 4.996f >= 4.995f): increase is disabled.
        mPdfToolbarCoordinator.onViewportChanged(0, 4.996f);
        assertTrue(zoomDecreaseButton.isEnabled());
        assertFalse(zoomIncreaseButton.isEnabled());

        // Beyond ZOOM_EPSILON below maximum zoom (e.g. 4.994f < 4.995f): increase is enabled.
        mPdfToolbarCoordinator.onViewportChanged(0, 4.994f);
        assertTrue(zoomDecreaseButton.isEnabled());
        assertTrue(zoomIncreaseButton.isEnabled());
    }

    @Test
    public void testZoomWithFloatingPointImprecision() {
        View zoomIncreaseButton = mPdfPageView.findViewById(R.id.zoom_increase_button);
        View zoomDecreaseButton = mPdfPageView.findViewById(R.id.zoom_decrease_button);

        // Slightly above 0.33f (e.g. 0.331f) matches 0.33f and should step down to 0.25f
        mPdfToolbarCoordinator.onViewportChanged(0, 0.331f);
        zoomDecreaseButton.performClick();
        verify(mDelegate).changeZoomLevel(AdditionalMatchers.eq(0.25f, 0.001f));

        // Slightly below 0.33f (e.g. 0.329f) matches 0.33f and should step up to 0.5f
        mPdfToolbarCoordinator.onViewportChanged(0, 0.329f);
        zoomIncreaseButton.performClick();
        verify(mDelegate).changeZoomLevel(AdditionalMatchers.eq(0.5f, 0.001f));

        // Slightly below 1.0f (e.g. 0.999f) matches 1.0f and should step up to 1.1f
        mPdfToolbarCoordinator.onViewportChanged(0, 0.999f);
        zoomIncreaseButton.performClick();
        verify(mDelegate).changeZoomLevel(AdditionalMatchers.eq(1.1f, 0.001f));

        // Slightly above 1.0f (e.g. 1.001f) matches 1.0f and should step down to 0.9f
        mPdfToolbarCoordinator.onViewportChanged(0, 1.001f);
        zoomDecreaseButton.performClick();
        verify(mDelegate).changeZoomLevel(AdditionalMatchers.eq(0.9f, 0.001f));
    }

    @Test
    public void testTwoPagesPerRowToggle_convertsDisplayZoomToEngineZoom() {
        PdfToolbarCoordinator coordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        coordinator.onDocumentLoaded(10, "test.pdf");
        coordinator.setDefaultZoomLevel(0.5f);
        coordinator.onViewportChanged(0, 1.0f); // display zoom = 1.0f / 0.5f = 2.0f

        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        moreMenuButton.performClick();

        View contentView = mSpyPopupWindow.getContentView();
        ListView listView = contentView.findViewById(R.id.menu_list);
        View itemView = listView.getAdapter().getView(0, null, listView);

        itemView.performClick();
        // Display zoom is 2.0f; engine zoom passed to delegate should be 2.0f * 0.5f = 1.0f
        verify(mDelegate)
                .toggleTwoPagesPerRow(eq(true), AdditionalMatchers.eq(1.0f, 0.001f), eq(0));
    }

    @Test
    public void testZoomInClick_recordsMetric() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "Android.Pdf.ToolbarAction", PdfToolbarAction.ZOOM_IN);
        View zoomIncreaseButton = mPdfPageView.findViewById(R.id.zoom_increase_button);
        zoomIncreaseButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testZoomOutClick_recordsMetric() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "Android.Pdf.ToolbarAction", PdfToolbarAction.ZOOM_OUT);
        View zoomDecreaseButton = mPdfPageView.findViewById(R.id.zoom_decrease_button);
        zoomDecreaseButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testFitToPageToggle_recordsMetric() {
        View fitToPageButton = mPdfPageView.findViewById(R.id.fit_to_page_button);

        // First click (fit to page)
        var histogramWatcherFitToPage =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.FIT_TO_PAGE);
        fitToPageButton.performClick();
        histogramWatcherFitToPage.assertExpected();

        // Second click (fit to width)
        var histogramWatcherFitToWidth =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.FIT_TO_WIDTH);
        fitToPageButton.performClick();
        histogramWatcherFitToWidth.assertExpected();
    }

    @Test
    public void testFitToPageViaMenu_recordsMetric() {
        // Layout narrow to hide fit-to-page button and show it in the menu
        setToolbarWidth(680);

        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        moreMenuButton.performClick();

        View contentView = mSpyPopupWindow.getContentView();
        ListView listView = contentView.findViewById(R.id.menu_list);
        View fitItemView = null;
        for (int i = 0; i < listView.getAdapter().getCount(); i++) {
            View itemView = listView.getAdapter().getView(i, null, listView);
            TextView textView = itemView.findViewById(R.id.menu_item_text);
            String text = textView.getText().toString();
            if (text.equals(mActivity.getString(R.string.pdf_fit_page))
                    || text.equals(mActivity.getString(R.string.pdf_fit_width))) {
                fitItemView = itemView;
                break;
            }
        }
        org.junit.Assert.assertNotNull("Fit to page menu item should be found", fitItemView);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.FIT_TO_PAGE);
        fitItemView.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testPageNumberEdit_recordsMetric() {
        EditText currentPage = mPdfPageView.findViewById(R.id.current_page);
        assertTrue(currentPage.requestFocus());
        currentPage.setText("50");

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.PAGE_NAVIGATION);
        currentPage.onEditorAction(android.view.inputmethod.EditorInfo.IME_ACTION_GO);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDocumentLoaded() {
        // Initial state from constructor is 99/100
        mPdfToolbarCoordinator.onDocumentLoaded(50, "test_title.pdf");
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        TextView pageCount = mPdfPageView.findViewById(R.id.page_count);
        // Current page remains 99 (default), total page count becomes 50
        assertEquals("99", currentPage.getText().toString());
        assertEquals("50", pageCount.getText().toString());
        TextView title = mPdfPageView.findViewById(R.id.pdf_title);
        assertEquals("test_title.pdf", title.getText().toString());
    }

    @Test
    public void testFitToPageToggle() {
        // Default current page is 99 (1-indexed), so pageIndex should be 98.
        View fitToPageButton = mPdfPageView.findViewById(R.id.fit_to_page_button);

        // Initial state: click triggers fit-to-page and changes state to fit-to-width.
        fitToPageButton.performClick();
        verify(mDelegate).toggleFitToPage(true, 98);

        // Second click triggers fit-to-width and changes state back to fit-to-page.
        fitToPageButton.performClick();
        verify(mDelegate).toggleFitToPage(false, 98);
    }

    @Test
    public void testTwoPagesPerRowToggle_viaMenu_toggleBehavior() {
        // 1. Initial State: Single Page View is active (TWO_PAGES_PER_ROW_ACTIVE = false)
        // Click more menu button
        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        org.junit.Assert.assertNotNull("More menu button should not be null", moreMenuButton);
        moreMenuButton.performClick();

        // Get content view
        View contentView = mSpyPopupWindow.getContentView();
        org.junit.Assert.assertNotNull("Popup content view should not be null", contentView);
        ListView listView = contentView.findViewById(R.id.menu_list);
        org.junit.Assert.assertNotNull("List view should be found", listView);

        // Verify first item is "Two-page view" and has NO checkmark
        View itemView = listView.getAdapter().getView(0, null, listView);
        TextView textView = itemView.findViewById(R.id.menu_item_text);
        assertEquals(
                mActivity.getString(R.string.pdf_two_page_view), textView.getText().toString());
        ImageView endIcon = itemView.findViewById(R.id.menu_item_end_icon);
        assertTrue(endIcon.getVisibility() == View.GONE || endIcon.getDrawable() == null);

        // Click "Two-page view" -> toggles to true
        itemView.performClick();
        verify(mDelegate).toggleTwoPagesPerRow(true, 1.0f, 98);
        verify(mSpyPopupWindow).dismiss();

        // 2. Second State: Two Page View is active (TWO_PAGES_PER_ROW_ACTIVE = true)
        // Reset the spy for the next popup window creation
        mSpyPopupWindow = spy(new ChromePopupWindow(mActivity));
        when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        // Click more menu button again
        moreMenuButton.performClick();

        contentView = mSpyPopupWindow.getContentView();
        listView = contentView.findViewById(R.id.menu_list);

        // Verify first item is now "Single page view" and has NO checkmark
        itemView = listView.getAdapter().getView(0, null, listView);
        textView = itemView.findViewById(R.id.menu_item_text);
        assertEquals(
                mActivity.getString(R.string.pdf_single_page_view), textView.getText().toString());
        endIcon = itemView.findViewById(R.id.menu_item_end_icon);
        assertTrue(endIcon.getVisibility() == View.GONE || endIcon.getDrawable() == null);

        // Click "Single page view" -> toggles to false
        itemView.performClick();
        verify(mDelegate).toggleTwoPagesPerRow(false, 1.0f, 98);
        verify(mSpyPopupWindow).dismiss();
    }

    @Test
    public void testTwoPagesPerRowToggle_beforeViewportChanged() {
        PdfToolbarCoordinator coordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        moreMenuButton.performClick();

        View contentView = mSpyPopupWindow.getContentView();
        ListView listView = contentView.findViewById(R.id.menu_list);
        View itemView = listView.getAdapter().getView(0, null, listView);

        itemView.performClick();
        verify(mDelegate).toggleTwoPagesPerRow(true, 1.0f, 0);
    }

    @Test
    public void testFitToPageToggle_beforeViewportChanged() {
        PdfToolbarCoordinator coordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        View fitToPageButton = mPdfPageView.findViewById(R.id.fit_to_page_button);
        fitToPageButton.performClick();
        verify(mDelegate).toggleFitToPage(true, 0);
    }

    @Test
    public void testTwoPagesPerRowReset() {
        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        moreMenuButton.performClick();

        View contentView = mSpyPopupWindow.getContentView();
        android.widget.ListView listView = contentView.findViewById(R.id.menu_list);

        View itemView = listView.getAdapter().getView(0, null, listView);
        itemView.performClick();

        mPdfToolbarCoordinator.resetTwoPagesPerRow();

        mSpyPopupWindow = spy(new ChromePopupWindow(mActivity));
        when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        moreMenuButton.performClick();
        contentView = mSpyPopupWindow.getContentView();
        listView = contentView.findViewById(R.id.menu_list);

        itemView = listView.getAdapter().getView(0, null, listView);
        TextView textView = itemView.findViewById(R.id.menu_item_text);
        assertEquals(
                mActivity.getString(R.string.pdf_two_page_view), textView.getText().toString());
    }

    @Test
    public void testOnDocumentLoaded_resetsTwoPagesPerRow() {
        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        moreMenuButton.performClick();

        View contentView = mSpyPopupWindow.getContentView();
        android.widget.ListView listView = contentView.findViewById(R.id.menu_list);

        View itemView = listView.getAdapter().getView(0, null, listView);
        itemView.performClick();

        mPdfToolbarCoordinator.onDocumentLoaded(100, "test_title.pdf");

        mSpyPopupWindow = spy(new ChromePopupWindow(mActivity));
        when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        moreMenuButton.performClick();
        contentView = mSpyPopupWindow.getContentView();
        listView = contentView.findViewById(R.id.menu_list);

        itemView = listView.getAdapter().getView(0, null, listView);
        TextView textView = itemView.findViewById(R.id.menu_item_text);
        assertEquals(
                mActivity.getString(R.string.pdf_two_page_view), textView.getText().toString());
    }

    @Test
    public void testAdaptiveHiding() {
        View downloadButton = mPdfPageView.findViewById(R.id.download_button);
        View fitToPageButton = mPdfPageView.findViewById(R.id.fit_to_page_button);
        View zoomDecreaseButton = mPdfPageView.findViewById(R.id.zoom_decrease_button);
        View currentPage = mPdfPageView.findViewById(R.id.current_page);
        View editButton = mPdfPageView.findViewById(R.id.edit_button);
        View title = mPdfPageView.findViewById(R.id.pdf_title);

        View centerGroup = mPdfPageView.findViewById(R.id.pdf_toolbar_group_center);
        View navZoomDivider = mPdfPageView.findViewById(R.id.nav_zoom_divider);
        View zoomFitDivider = mPdfPageView.findViewById(R.id.zoom_fit_divider);
        View fitEditDivider = mPdfPageView.findViewById(R.id.fit_edit_divider);

        // State 1: Wide screen (e.g. 900dp) -> All should be visible
        setToolbarWidth(900);

        assertEquals(
                PdfUtils.isInlinePdfV2Enabled() ? View.VISIBLE : View.GONE,
                downloadButton.getVisibility());
        assertEquals(View.VISIBLE, fitToPageButton.getVisibility());
        assertEquals(View.VISIBLE, zoomDecreaseButton.getVisibility());
        assertEquals(View.VISIBLE, currentPage.getVisibility());
        assertEquals(View.VISIBLE, editButton.getVisibility());
        assertEquals(View.VISIBLE, navZoomDivider.getVisibility());
        assertEquals(View.VISIBLE, zoomFitDivider.getVisibility());
        assertEquals(View.VISIBLE, fitEditDivider.getVisibility());
        assertEquals(View.VISIBLE, centerGroup.getVisibility());

        // Verify title is constrained to center group
        ConstraintLayout.LayoutParams layoutParams =
                (ConstraintLayout.LayoutParams) title.getLayoutParams();
        assertEquals(R.id.pdf_toolbar_group_center, layoutParams.endToStart);

        // State 2: Narrower (e.g. 780dp) -> Download should be GONE, others VISIBLE
        setToolbarWidth(780);
        assertEquals(View.GONE, downloadButton.getVisibility());
        assertEquals(View.VISIBLE, fitToPageButton.getVisibility());
        assertEquals(View.VISIBLE, zoomDecreaseButton.getVisibility());
        assertEquals(View.VISIBLE, currentPage.getVisibility());
        assertEquals(View.VISIBLE, editButton.getVisibility());
        assertEquals(View.VISIBLE, navZoomDivider.getVisibility());
        assertEquals(View.VISIBLE, zoomFitDivider.getVisibility());
        assertEquals(View.VISIBLE, fitEditDivider.getVisibility());
        assertEquals(View.VISIBLE, centerGroup.getVisibility());

        // State 3: Narrower (e.g. 720dp) -> Download GONE, others VISIBLE (was Download and Rotate
        // GONE)
        setToolbarWidth(720);
        assertEquals(View.GONE, downloadButton.getVisibility());
        assertEquals(View.VISIBLE, fitToPageButton.getVisibility());
        assertEquals(View.VISIBLE, zoomDecreaseButton.getVisibility());
        assertEquals(View.VISIBLE, currentPage.getVisibility());
        assertEquals(View.VISIBLE, editButton.getVisibility());
        assertEquals(View.VISIBLE, navZoomDivider.getVisibility());
        assertEquals(View.VISIBLE, zoomFitDivider.getVisibility());
        assertEquals(View.VISIBLE, fitEditDivider.getVisibility());
        assertEquals(View.VISIBLE, centerGroup.getVisibility());

        // State 4: Narrower (e.g. 680dp) -> Download, Fit GONE (was Download, Rotate, Fit GONE)
        setToolbarWidth(680);
        assertEquals(View.GONE, downloadButton.getVisibility());
        assertEquals(View.GONE, fitToPageButton.getVisibility());
        assertEquals(View.VISIBLE, zoomDecreaseButton.getVisibility());
        assertEquals(View.VISIBLE, currentPage.getVisibility());
        assertEquals(View.VISIBLE, editButton.getVisibility());
        assertEquals(View.VISIBLE, navZoomDivider.getVisibility());
        assertEquals(View.GONE, zoomFitDivider.getVisibility());
        assertEquals(View.GONE, fitEditDivider.getVisibility());
        assertEquals(View.VISIBLE, centerGroup.getVisibility());

        // State 5: Narrower (e.g. 620dp) -> Download, Fit, Zoom GONE (was Download, Rotate, Fit,
        // Zoom GONE)
        setToolbarWidth(620);
        assertEquals(View.GONE, downloadButton.getVisibility());
        assertEquals(View.GONE, fitToPageButton.getVisibility());
        assertEquals(View.GONE, zoomDecreaseButton.getVisibility());
        assertEquals(View.VISIBLE, currentPage.getVisibility());
        assertEquals(View.VISIBLE, editButton.getVisibility());
        assertEquals(View.GONE, navZoomDivider.getVisibility());
        assertEquals(View.GONE, zoomFitDivider.getVisibility());
        assertEquals(View.GONE, fitEditDivider.getVisibility());
        assertEquals(View.VISIBLE, centerGroup.getVisibility());

        // State 6: Most narrow (e.g. 550dp) -> All center gone, only print/menu/title remain
        setToolbarWidth(550);
        assertEquals(View.GONE, downloadButton.getVisibility());
        assertEquals(View.GONE, fitToPageButton.getVisibility());
        assertEquals(View.GONE, zoomDecreaseButton.getVisibility());
        assertEquals(View.GONE, currentPage.getVisibility());
        assertEquals(View.GONE, editButton.getVisibility());
        assertEquals(View.GONE, navZoomDivider.getVisibility());
        assertEquals(View.GONE, zoomFitDivider.getVisibility());
        assertEquals(View.GONE, fitEditDivider.getVisibility());
        assertEquals(View.GONE, centerGroup.getVisibility());

        // Print and More menu should still be visible
        View printButton = mPdfPageView.findViewById(R.id.print_button);
        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        assertEquals(View.VISIBLE, printButton.getVisibility());
        assertEquals(View.VISIBLE, moreMenuButton.getVisibility());

        // Verify title is now constrained to end group
        layoutParams = (ConstraintLayout.LayoutParams) title.getLayoutParams();
        assertEquals(R.id.pdf_toolbar_group_end, layoutParams.endToStart);
    }

    @Test
    public void testGetNextEngineZoomLevel_increase() {
        // Current zoom is 1.0f
        mPdfToolbarCoordinator.onViewportChanged(98, 1.0f);
        assertEquals(1.1f, mPdfToolbarCoordinator.getNextEngineZoomLevel(true), 0.001f);

        // Zoom level not in list: 1.05f
        mPdfToolbarCoordinator.onViewportChanged(98, 1.05f);
        assertEquals(1.1f, mPdfToolbarCoordinator.getNextEngineZoomLevel(true), 0.001f);
    }

    @Test
    public void testGetNextEngineZoomLevel_decrease() {
        // Current zoom is 1.0f
        mPdfToolbarCoordinator.onViewportChanged(98, 1.0f);
        assertEquals(0.9f, mPdfToolbarCoordinator.getNextEngineZoomLevel(false), 0.001f);

        // Zoom level not in list: 1.05f
        mPdfToolbarCoordinator.onViewportChanged(98, 1.05f);
        assertEquals(1.0f, mPdfToolbarCoordinator.getNextEngineZoomLevel(false), 0.001f);
    }

    @Test
    public void testGetNextEngineZoomLevel_boundary() {
        // Max zoom is 5.0f
        mPdfToolbarCoordinator.onViewportChanged(98, 5.0f);
        org.junit.Assert.assertNull(mPdfToolbarCoordinator.getNextEngineZoomLevel(true));

        // Min zoom is 0.25f
        mPdfToolbarCoordinator.onViewportChanged(98, 0.25f);
        org.junit.Assert.assertNull(mPdfToolbarCoordinator.getNextEngineZoomLevel(false));
    }

    @Test
    public void testPrintButtonClick() {
        View printButton = mPdfPageView.findViewById(R.id.print_button);
        org.junit.Assert.assertNotNull("Print button should not be null", printButton);
        printButton.performClick();
        verify(mDelegate).print();
    }

    @Test
    public void testEditButtonClick() {
        View editButton = mPdfPageView.findViewById(R.id.edit_button);
        org.junit.Assert.assertNotNull("Edit button should not be null", editButton);

        // Initial state: EDIT_MODE_ACTIVE is false (default)
        // Click should enter edit mode, calling setEditMode(true)
        editButton.performClick();
        verify(mDelegate).setEditMode(true);

        // Now set the model to active (simulating delegate callback -> coordinator -> model update)
        mPdfToolbarCoordinator.setEditModeActive(true);

        // Click again while already in edit mode: should NOT exit edit mode or call setEditMode(false)
        editButton.performClick();
        verify(mDelegate, never()).setEditMode(false);
    }

    @Test
    public void testEditButton_EditDisabled() {
        PdfUtils.setInlinePdfV2EditEnabledForTesting(false);
        View pageView = LayoutInflater.from(mActivity).inflate(R.layout.pdf_page, null);
        PdfToolbarCoordinator coordinator = new PdfToolbarCoordinator(pageView, mDelegate);
        PdfToolbar toolbar = pageView.findViewById(R.id.pdf_toolbar);
        View editButton = pageView.findViewById(R.id.edit_button);
        View fitEditDivider = pageView.findViewById(R.id.fit_edit_divider);

        // Wide screen (e.g. 900dp) -> Edit button and fit_edit_divider should be GONE when edit is disabled
        setToolbarWidth(toolbar, 900);

        assertEquals(View.GONE, editButton.getVisibility());
        assertEquals(View.GONE, fitEditDivider.getVisibility());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2_DOWNLOAD)
    public void testDownloadButton_FeatureDisabled() {
        View downloadButton = mPdfPageView.findViewById(R.id.download_button);

        // Wide screen (e.g. 900dp) -> Should still be GONE because feature is disabled
        setToolbarWidth(900);

        assertEquals(View.GONE, downloadButton.getVisibility());
    }

    @Test
    public void testDoneButtonVisibilityAndClick() {
        View doneButton = mPdfPageView.findViewById(R.id.done_button);
        View editButton = mPdfPageView.findViewById(R.id.edit_button);
        org.junit.Assert.assertNotNull("Done button should not be null", doneButton);

        // 1. Initial State: Edit mode inactive, wide screen -> Done button GONE
        setToolbarWidth(900);
        assertEquals(View.GONE, doneButton.getVisibility());

        // 2. Wide screen, Edit mode active -> Done button VISIBLE (along with edit button)
        mPdfToolbarCoordinator.setEditModeActive(true);
        assertEquals(View.VISIBLE, editButton.getVisibility());
        assertEquals(View.VISIBLE, doneButton.getVisibility());

        // 3. Narrow screen (edit button hidden), Edit mode active -> Done button still VISIBLE
        setToolbarWidth(550);
        assertEquals(View.GONE, editButton.getVisibility());
        assertEquals(View.VISIBLE, doneButton.getVisibility());

        // 4. Click Done button -> should call setEditMode(false)
        doneButton.performClick();
        verify(mDelegate).setEditMode(false);

        // 5. Narrow screen, Edit mode inactive -> Done button GONE
        mPdfToolbarCoordinator.setEditModeActive(false);
        assertEquals(View.GONE, doneButton.getVisibility());
    }

    @Test
    public void testPrintButtonClick_recordsMetric() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.PRINT);
        View printButton = mPdfPageView.findViewById(R.id.print_button);
        printButton.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testTwoPagesPerRowToggle_viaMenu_recordsMetric() {
        // Initial state is single page view (two page view inactive)
        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        moreMenuButton.performClick();

        View contentView = mSpyPopupWindow.getContentView();
        ListView listView = contentView.findViewById(R.id.menu_list);
        View itemView = listView.getAdapter().getView(0, null, listView); // Two-page view item

        // Click "Two-page view" -> toggles to true, should record TWO_PAGE_VIEW
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.TWO_PAGE_VIEW);
        itemView.performClick();
        histogramWatcher.assertExpected();

        // Reset the spy for the next popup window creation
        mSpyPopupWindow = spy(new ChromePopupWindow(mActivity));
        when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        // Click more menu button again
        moreMenuButton.performClick();

        contentView = mSpyPopupWindow.getContentView();
        listView = contentView.findViewById(R.id.menu_list);
        itemView = listView.getAdapter().getView(0, null, listView); // Single page view item

        // Click "Single page view" -> toggles to false, should record SINGLE_PAGE_VIEW
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.SINGLE_PAGE_VIEW);
        itemView.performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDocumentPropertiesClick_recordsMetric() {
        View moreMenuButton = mPdfPageView.findViewById(R.id.more_menu_button);
        moreMenuButton.performClick();

        View contentView = mSpyPopupWindow.getContentView();
        ListView listView = contentView.findViewById(R.id.menu_list);
        View propertiesItemView = null;
        for (int i = 0; i < listView.getAdapter().getCount(); i++) {
            View itemView = listView.getAdapter().getView(i, null, listView);
            TextView textView = itemView.findViewById(R.id.menu_item_text);
            if (textView.getText()
                    .toString()
                    .equals(mActivity.getString(R.string.pdf_document_properties))) {
                propertiesItemView = itemView;
                break;
            }
        }
        org.junit.Assert.assertNotNull(
                "Document properties menu item should be found", propertiesItemView);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.ToolbarAction", PdfToolbarAction.DOCUMENT_PROPERTIES);
        propertiesItemView.performClick();
        histogramWatcher.assertExpected();
    }
}
