// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
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

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mPdfPageView = LayoutInflater.from(mActivity).inflate(R.layout.pdf_page, null);
        mPdfToolbarCoordinator = new PdfToolbarCoordinator(mPdfPageView, mDelegate);
        mPdfToolbarCoordinator.onDocumentLoaded(100, "test_title.pdf");
        mPdfToolbarCoordinator.onViewportChanged(98, 1); // 0-indexed page 98

    }

    @After
    public void tearDown() throws Exception {
        mCloseableMocks.close();
    }

    @Test
    public void testPageIncrease() {
        // Default current page is 99 (1-indexed)
        View pageIncreaseButton = mPdfPageView.findViewById(R.id.page_increase_button);
        pageIncreaseButton.performClick();
        verify(mDelegate).navigateToPage(99);
    }

    @Test
    public void testPageDecrease() {
        // Default current page is 99 (1-indexed)
        View pageDecreaseButton = mPdfPageView.findViewById(R.id.page_decrease_button);
        pageDecreaseButton.performClick();
        verify(mDelegate).navigateToPage(97);
    }

    @Test
    public void testViewportChanged() {
        mPdfToolbarCoordinator.onViewportChanged(5, 1);
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        TextView pageCountDivider = mPdfPageView.findViewById(R.id.page_count_divider);
        TextView pageCount = mPdfPageView.findViewById(R.id.page_count);
        TextView zoomValue = mPdfPageView.findViewById(R.id.zoom_value);
        // Current page is firstVisiblePage + 1
        Assert.assertEquals(
                "6 / 100",
                currentPage.getText().toString()
                        + pageCountDivider.getText().toString()
                        + pageCount.getText().toString());
        Assert.assertEquals("100%", zoomValue.getText().toString());
    }

    @Test
    public void testOnViewportChanged_indexing() {
        // Input is 0-indexed (page 0), output should be 1-indexed ("1")
        mPdfToolbarCoordinator.onViewportChanged(0, 1);
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        Assert.assertEquals("1", currentPage.getText().toString());

        // Input is 0-indexed (page 5), output should be 1-indexed ("6")
        mPdfToolbarCoordinator.onViewportChanged(5, 1);
        Assert.assertEquals("6", currentPage.getText().toString());
    }

    @Test
    public void testViewInit() {
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        TextView pageCountDivider = mPdfPageView.findViewById(R.id.page_count_divider);
        TextView pageCount = mPdfPageView.findViewById(R.id.page_count);
        Assert.assertEquals(
                "99 / 100",
                currentPage.getText().toString()
                        + pageCountDivider.getText().toString()
                        + pageCount.getText().toString());
        TextView zoomValue = mPdfPageView.findViewById(R.id.zoom_value);
        Assert.assertEquals("100%", zoomValue.getText().toString());
    }

    @Test
    public void testOnDocumentLoaded() {
        // Initial state from constructor is 99/100
        mPdfToolbarCoordinator.onDocumentLoaded(50, "test_title.pdf");
        TextView currentPage = mPdfPageView.findViewById(R.id.current_page);
        TextView pageCount = mPdfPageView.findViewById(R.id.page_count);
        // Current page remains 99 (default), total page count becomes 50
        Assert.assertEquals("99", currentPage.getText().toString());
        Assert.assertEquals("50", pageCount.getText().toString());
        TextView title = mPdfPageView.findViewById(R.id.pdf_title);
        Assert.assertEquals("test_title.pdf", title.getText().toString());
    }
}
