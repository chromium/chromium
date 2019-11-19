// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.SERVER_CERT_PEM;
import static org.chromium.net.CronetTestRule.SERVER_KEY_PKCS8_PEM;
import static org.chromium.net.CronetTestRule.getContext;

import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;

/**
 * Tests requests that generate Network Error Logging reports.
 */
@RunWith(AndroidJUnit4.class)
public class NetworkErrorLoggingTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        TestFilesInstaller.installIfNeeded(getContext());
        assertTrue(Http2TestServer.startHttp2TestServer(
                getContext(), SERVER_CERT_PEM, SERVER_KEY_PKCS8_PEM));
    }

    @After
    public void tearDown() throws Exception {
        assertTrue(Http2TestServer.shutdownHttp2TestServer());
        if (mCronetEngine != null) {
            mCronetEngine.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testManualReportUpload() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mCronetEngine = builder.build();
        String url = Http2TestServer.getReportingCollectorUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("[{\"type\": \"test_report\"}]".getBytes());
        requestBuilder.setUploadDataProvider(dataProvider, callback.getExecutor());
        requestBuilder.addHeader("Content-Type", "application/reports+json");
        requestBuilder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertTrue(Http2TestServer.getReportingCollector().containsReport(
                "{\"type\": \"test_report\"}"));
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testUploadNELReportsFromHeaders() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.setExperimentalOptions("{\"NetworkErrorLogging\": {\"enable\": true}}");
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mCronetEngine = builder.build();
        String url = Http2TestServer.getSuccessWithNELHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        Http2TestServer.getReportingCollector().waitForReports(1);
        assertTrue(Http2TestServer.getReportingCollector().containsReport(""
                + "{"
                + "  \"type\": \"network-error\","
                + "  \"url\": \"" + url + "\","
                + "  \"body\": {"
                + "    \"method\": \"GET\","
                + "    \"phase\": \"application\","
                + "    \"protocol\": \"h2\","
                + "    \"referrer\": \"\","
                + "    \"sampling_fraction\": 1.0,"
                + "    \"server_ip\": \"127.0.0.1\","
                + "    \"status_code\": 200,"
                + "    \"type\": \"ok\""
                + "  }"
                + "}"));
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testUploadNELReportsFromPreloadedPolicy() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        String serverOrigin = Http2TestServer.getServerUrl();
        String collectorUrl = Http2TestServer.getReportingCollectorUrl();
        builder.setExperimentalOptions(""
                + "{\"NetworkErrorLogging\": {"
                + "  \"enable\": true,"
                + "  \"preloaded_report_to_headers\": ["
                + "    {"
                + "      \"origin\": \"" + serverOrigin + "\","
                + "      \"value\": {"
                + "        \"group\": \"nel\","
                + "        \"max_age\": 86400,"
                + "        \"endpoints\": ["
                + "          {\"url\": \"" + collectorUrl + "\"}"
                + "        ]"
                + "      }"
                + "    }"
                + "  ],"
                + "  \"preloaded_nel_headers\": ["
                + "    {"
                + "      \"origin\": \"" + serverOrigin + "\","
                + "      \"value\": {"
                + "        \"report_to\": \"nel\","
                + "        \"max_age\": 86400,"
                + "        \"success_fraction\": 1.0"
                + "      }"
                + "    }"
                + "  ]"
                + "}}");
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mCronetEngine = builder.build();
        String url = Http2TestServer.getEchoMethodUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        Http2TestServer.getReportingCollector().waitForReports(1);
        // Note that because we don't know in advance what the server IP address is for preloaded
        // origins, we'll always get a "downgraded" dns.address_changed NEL report if we don't
        // receive a replacement NEL policy with the request.
        assertTrue(Http2TestServer.getReportingCollector().containsReport(""
                + "{"
                + "  \"type\": \"network-error\","
                + "  \"url\": \"" + url + "\","
                + "  \"body\": {"
                + "    \"method\": \"GET\","
                + "    \"phase\": \"dns\","
                + "    \"protocol\": \"h2\","
                + "    \"referrer\": \"\","
                + "    \"sampling_fraction\": 1.0,"
                + "    \"server_ip\": \"127.0.0.1\","
                + "    \"status_code\": 0,"
                + "    \"type\": \"dns.address_changed\""
                + "  }"
                + "}"));
    }
}
