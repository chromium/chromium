// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ukm;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;

@RunWith(BaseRobolectricTestRunner.class)
public final class UkmRecorderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock WebContents mWebContents;
    private @Mock UkmRecorder.Natives mUkmRecorderJniMock;

    private static final String TEST_EVENT_NAME = "event1";
    private static final String METRIC_NAME1 = "metricName1";
    private static final String METRIC_NAME2 = "metricName2";

    @Before
    public void setUp() {
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);
    }

    @Test
    public void record_multipleMetrics() {
        new UkmRecorder(mWebContents, TEST_EVENT_NAME)
                .addMetric(METRIC_NAME1, 5)
                .addMetric(METRIC_NAME2, 10)
                .record();
        verify(mUkmRecorderJniMock)
                .recordEventWithMultipleMetrics(
                        any(),
                        eq(TEST_EVENT_NAME),
                        argThat(
                                metricsList ->
                                        metricsList.length == 2
                                                && metricsList[0].mName.equals(METRIC_NAME1)
                                                && metricsList[0].mValue == 5
                                                && metricsList[1].mName.equals(METRIC_NAME2)
                                                && metricsList[1].mValue == 10));
    }
}
