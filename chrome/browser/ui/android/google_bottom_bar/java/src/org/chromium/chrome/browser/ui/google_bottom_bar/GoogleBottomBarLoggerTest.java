// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BOTTOM_BAR_CREATED_HISTOGRAM;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;

/** Unit tests for {@link GoogleBottomBarLogger}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GoogleBottomBarLoggerTest {

    private HistogramWatcher mHistogramWatcher;

    @After
    public void tearDown() {
        if (mHistogramWatcher != null) {
            mHistogramWatcher.assertExpected();
            mHistogramWatcher.close();
            mHistogramWatcher = null;
        }
    }

    @Test
    public void logCreatedEvent_evenLayout_logsEvenLayout() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.EVEN_LAYOUT);

        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.EVEN_LAYOUT);
    }

    @Test
    public void logCreatedEvent_spotlightLayout_logsSpotlightLayout() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);

        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);
    }

    @Test(expected = AssertionError.class)
    public void logCreatedEvent_unsupportedLayout_doesNotLogEvent() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BOTTOM_BAR_CREATED_HISTOGRAM).build();

        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.COUNT);
    }
}
