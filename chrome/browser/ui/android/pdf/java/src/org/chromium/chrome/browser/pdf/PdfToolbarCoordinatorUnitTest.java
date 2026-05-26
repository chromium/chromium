// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.UiWidgetFactory;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
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
    private PopupWindow mSpyPopupWindow;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mMockUiWidgetFactory = mock(UiWidgetFactory.class);
        mSpyPopupWindow = spy(new PopupWindow(mActivity));
        UiWidgetFactory.setInstance(mMockUiWidgetFactory);
        when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

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
    // clicks zoom-in, getNextZoomLevel(5.0f, true) used to throw IndexOutOfBoundsException
    // because the while-loop advanced index to mZoomLevels.size().
    @Test
    public void testZoomIncrease_atMaxZoom_doesNotCrash() {
        // 4.999f < 5.0f, so the zoom-increase button is enabled...
        mPdfToolbarCoordinator.onViewportChanged(0, 4.999f);
        // ...but "%.0f%%" rounds 499.9 → "500%", which parses back to 5.0f.
        View zoomIncreaseButton = mPdfPageView.findViewById(R.id.zoom_increase_button);
        // Should not throw and should clamp to the maximum zoom level (5.0f).
        zoomIncreaseButton.performClick();
        verify(mDelegate).changeZoomLevel(5.0f);
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

        // Initial state: click triggers fit-to-height and changes state to fit-to-width.
        fitToPageButton.performClick();
        verify(mDelegate).toggleFitToPage(true, 98);

        // Second click triggers fit-to-width and changes state back to fit-to-height.
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
        android.widget.ListView listView = contentView.findViewById(R.id.menu_list);
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
        mSpyPopupWindow = spy(new PopupWindow(mActivity));
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
}
