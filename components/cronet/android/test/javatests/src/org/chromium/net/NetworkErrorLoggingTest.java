// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.test.Protocol;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.Type;

import java.util.List;

/** Tests requests that generate Network Error Logging reports. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason = "Fallback and AOSP implementations do not support network error logging")
public class NetworkErrorLoggingTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private static final String REPORTING_COLLECTOR_PATH = "/reporting-collector";
    private static final String SUCCESS_WITH_NEL_HEADERS_PATH = "/success-with-nel";

    private NativeTestServer mTestServer;
    private final ReportingCollector mReportingCollector = new ReportingCollector();

    @Before
    public void setUp() throws Exception {
        mTestServer =
                new NativeTestServer(
                        mTestRule.getTestFramework().getContext(), Type.HTTPS, Protocol.HTTP2);
        mTestServer.setSSLConfig(ServerCertificate.CERT_COMMON_NAME_IS_DOMAIN);
        mTestServer.registerRequestHandler(this::handleRequest);
        mTestServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.close();
    }

    private NativeTestServer.RawHttpResponse handleRequest(NativeTestServer.HttpRequest request) {
        switch (request.getRelativeUrl()) {
            case REPORTING_COLLECTOR_PATH:
                return NativeTestServer.RawHttpResponse.createFromHeaders(
                        List.of(
                                mReportingCollector.addReports(request.getContent())
                                        ? "HTTP/1.1 200 OK"
                                        : "HTTP/1.1 400 Bad Request"));
            case SUCCESS_WITH_NEL_HEADERS_PATH:
                return NativeTestServer.RawHttpResponse.createFromHeaders(
                        List.of(
                                "HTTP/1.1 200 OK",
                                String.format(
                                        "report-to: {\"group\": \"nel\", \"max_age\": 86400, "
                                                + "\"endpoints\": [{\"url\": \"%s\"}]}",
                                        getReportingCollectorUrl()),
                                "nel: {\"report_to\": \"nel\", \"max_age\": 86400, "
                                        + "\"success_fraction\": 1.0}"));
            default:
                return NativeTestServer.RawHttpResponse.createFromHeaders(
                        List.of("HTTP/1.1 404 Not Found"));
        }
    }

    private String getReportingCollectorUrl() {
        return mTestServer.getFileURL(REPORTING_COLLECTOR_PATH);
    }

    @Test
    @SmallTest
    public void testManualReportUpload() throws Exception {
        String url = getReportingCollectorUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mTestRule
                        .getTestFramework()
                        .startEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("[{\"type\": \"test_report\"}]".getBytes());
        requestBuilder.setUploadDataProvider(dataProvider, callback.getExecutor());
        requestBuilder.addHeader("Content-Type", "application/reports+json");
        requestBuilder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        mReportingCollector.assertContainsReport("{\"type\": \"test_report\"}");
    }

    @Test
    @SmallTest
    public void testUploadNELReportsFromHeaders() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.setExperimentalOptions(
                                    "{\"NetworkErrorLogging\": {\"enable\": true}}");
                        });
        String url = mTestServer.getFileURL(SUCCESS_WITH_NEL_HEADERS_PATH);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mTestRule
                        .getTestFramework()
                        .startEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        mReportingCollector.waitForReports(1);
        mReportingCollector.assertContainsReport(
                ""
                        + "{"
                        + "  \"type\": \"network-error\","
                        + "  \"url\": \""
                        + url
                        + "\","
                        + "  \"body\": {"
                        + "    \"method\": \"GET\","
                        + "    \"phase\": \"application\","
                        + "    \"protocol\": \"h2\","
                        + "    \"referrer\": \"\","
                        + "    \"sampling_fraction\": 1.0,"
                        + "    \"status_code\": 200,"
                        + "    \"type\": \"ok\""
                        + "  }"
                        + "}");
    }

    @Test
    @SmallTest
    public void testUploadNELReportsFromPreloadedPolicy() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            String serverOrigin = "https://" + mTestServer.getHostPort();
                            String collectorUrl = getReportingCollectorUrl();
                            builder.setExperimentalOptions(
                                    ""
                                            + "{\"NetworkErrorLogging\": {"
                                            + "  \"enable\": true,"
                                            + "  \"preloaded_report_to_headers\": ["
                                            + "    {"
                                            + "      \"origin\": \""
                                            + serverOrigin
                                            + "\","
                                            + "      \"value\": {"
                                            + "        \"group\": \"nel\","
                                            + "        \"max_age\": 86400,"
                                            + "        \"endpoints\": ["
                                            + "          {\"url\": \""
                                            + collectorUrl
                                            + "\"}"
                                            + "        ]"
                                            + "      }"
                                            + "    }"
                                            + "  ],"
                                            + "  \"preloaded_nel_headers\": ["
                                            + "    {"
                                            + "      \"origin\": \""
                                            + serverOrigin
                                            + "\","
                                            + "      \"value\": {"
                                            + "        \"report_to\": \"nel\","
                                            + "        \"max_age\": 86400,"
                                            + "        \"success_fraction\": 1.0"
                                            + "      }"
                                            + "    }"
                                            + "  ]"
                                            + "}}");
                        });

        String url = mTestServer.getEchoMethodURL();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mTestRule
                        .getTestFramework()
                        .startEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        mReportingCollector.waitForReports(1);
        // Note that because we don't know in advance what the server IP address is for preloaded
        // origins, we'll always get a "downgraded" dns.address_changed NEL report if we don't
        // receive a replacement NEL policy with the request.
        mReportingCollector.assertContainsReport(
                ""
                        + "{"
                        + "  \"type\": \"network-error\","
                        + "  \"url\": \""
                        + url
                        + "\","
                        + "  \"body\": {"
                        + "    \"method\": \"GET\","
                        + "    \"phase\": \"dns\","
                        + "    \"protocol\": \"h2\","
                        + "    \"referrer\": \"\","
                        + "    \"sampling_fraction\": 1.0,"
                        + "    \"status_code\": 0,"
                        + "    \"type\": \"dns.address_changed\""
                        + "  }"
                        + "}");
    }
}
