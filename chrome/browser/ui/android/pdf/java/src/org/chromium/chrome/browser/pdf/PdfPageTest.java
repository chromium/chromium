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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfLoadResult;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.PdfCtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.net.test.EmbeddedTestServer;

/** Test for user flows around {@link PdfPage}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(VERSION_CODES.VANILLA_ICE_CREAM)
public class PdfPageTest {
    private static final long TIMEOUT_MS = 8000;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;

    @Before
    public void setup() {
        mPage = mActivityTestRule.startOnBlankPage();
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
        PdfCtaPageStation pdfPage = mPage.openFakeLink(url, PdfCtaPageStation.newBuilder());
        histogramExpectation.assertExpected();
    }
}
