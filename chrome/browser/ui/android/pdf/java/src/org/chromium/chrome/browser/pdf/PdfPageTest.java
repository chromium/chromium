// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfLoadResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;

/** Test for user flows around {@link PdfPage}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(VERSION_CODES.VANILLA_ICE_CREAM)
public class PdfPageTest {
    private static final long TIMEOUT_MS = 8000;
    private static final long POLLING_INTERVAL_MS = 500;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setup() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    public void testLoadPdfUrl() {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/pdf/test/data/hello_world2.pdf");
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.Pdf.DocumentLoad", true)
                        .expectIntRecord(
                                "Android.Pdf.DocumentLoadResult.Detail", PdfLoadResult.SUCCESS)
                        .build();
        Tab tab = mActivityTestRule.loadUrlInNewTab(url);
        CriteriaHelper.pollUiThread(
                () -> {
                    if (!tab.isNativePage()) {
                        return false;
                    }
                    NativePage page = tab.getNativePage();
                    if (page instanceof PdfPage) {
                        return ((PdfPage) page)
                                .mPdfCoordinator
                                .mChromePdfViewerFragment
                                .mIsLoadDocumentSuccess;
                    } else {
                        return false;
                    }
                },
                "PDF document is not loaded successfully.",
                TIMEOUT_MS,
                POLLING_INTERVAL_MS);
        histogramExpectation.assertExpected();
    }
}
