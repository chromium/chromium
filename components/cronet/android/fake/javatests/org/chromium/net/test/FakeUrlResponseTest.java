// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.UrlResponseInfoImpl;

import java.io.UnsupportedEncodingException;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Test functionality of FakeUrlResponse. */
@RunWith(AndroidJUnit4.class)
public class FakeUrlResponseTest {
    private static final int TEST_HTTP_STATUS_CODE = 201;
    private static final String TEST_HEADER_NAME = "name";
    private static final String TEST_HEADER_VALUE = "value";
    private static final boolean TEST_WAS_CACHED = true;
    private static final String TEST_NEGOTIATED_PROTOCOL = "test_negotiated_protocol";
    private static final String TEST_PROXY_SERVER = "test_proxy_server";
    private static final String TEST_BODY = "test_body";

    List<Map.Entry<String, String>> mTestHeaders;
    AbstractMap.SimpleEntry<String, String> mTestHeaderEntry;
    FakeUrlResponse mTestResponse;

    @Before
    public void setUp() {
        mTestHeaders = new ArrayList<>();
        mTestHeaderEntry = new AbstractMap.SimpleEntry<>(TEST_HEADER_NAME, TEST_HEADER_VALUE);
        mTestHeaders.add(mTestHeaderEntry);
        mTestResponse =
                new FakeUrlResponse.Builder()
                        .setHttpStatusCode(TEST_HTTP_STATUS_CODE)
                        .setWasCached(TEST_WAS_CACHED)
                        .addHeader(TEST_HEADER_NAME, TEST_HEADER_VALUE)
                        .setNegotiatedProtocol(TEST_NEGOTIATED_PROTOCOL)
                        .setProxyServer(TEST_PROXY_SERVER)
                        .setResponseBody(TEST_BODY.getBytes())
                        .build();
    }

    @Test
    @SmallTest
    public void testAddHeader() {
        FakeUrlResponse response =
                new FakeUrlResponse.Builder()
                        .addHeader(TEST_HEADER_NAME, TEST_HEADER_VALUE)
                        .build();

        List<Map.Entry<String, String>> responseHeadersList = response.getAllHeadersList();

        // mTestHeaderEntry is header entry of TEST_HEADER_NAME, TEST_HEADER_VALUE.
        assertThat(responseHeadersList).contains(mTestHeaderEntry);
    }

    @Test
    @SmallTest
    public void testEquals() {
        FakeUrlResponse responseEqualToTestResponse = mTestResponse.toBuilder().build();
        FakeUrlResponse responseNotEqualToTestResponse =
                mTestResponse.toBuilder().setResponseBody("Not equal".getBytes()).build();

        assertThat(mTestResponse).isEqualTo(responseEqualToTestResponse);
        assertThat(mTestResponse).isNotEqualTo(responseNotEqualToTestResponse);
    }

    @Test
    @SmallTest
    public void testResponseBodyIsSame() {
        try {
            FakeUrlResponse responseWithBodySetAsBytes =
                    mTestResponse.toBuilder().setResponseBody(TEST_BODY.getBytes("UTF-8")).build();
            assertThat(mTestResponse.getResponseBody())
                    .isEqualTo(responseWithBodySetAsBytes.getResponseBody());
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(
                    "Exception occurred while encoding response body: " + TEST_BODY);
        }
    }

    @Test
    @SmallTest
    public void testHeadersNotShared() {
        FakeUrlResponse.Builder responseBuilder = new FakeUrlResponse.Builder();
        FakeUrlResponse response = responseBuilder.build();
        FakeUrlResponse responseWithHeader =
                responseBuilder.addHeader(TEST_HEADER_NAME, TEST_HEADER_VALUE).build();
        List<Map.Entry<String, String>> responseHeadersList = response.getAllHeadersList();
        List<Map.Entry<String, String>> responseHeadersListWithHeader =
                responseWithHeader.getAllHeadersList();

        assertThat(responseHeadersList).isNotEqualTo(responseHeadersListWithHeader);
    }

    @Test
    @SmallTest
    public void testSettingAllHeadersCopiesHeaderList() {
        String nameNotInOriginalList = "nameNotInOriginalList";
        String valueNotInOriginalList = "valueNotInOriginalList";
        AbstractMap.SimpleEntry<String, String> entryNotInOriginalList =
                new AbstractMap.SimpleEntry<>(nameNotInOriginalList, valueNotInOriginalList);

        FakeUrlResponse testResponseWithHeader =
                mTestResponse.toBuilder()
                        .addHeader(nameNotInOriginalList, valueNotInOriginalList)
                        .build();

        assertThat(mTestHeaders).doesNotContain(entryNotInOriginalList);
        assertThat(testResponseWithHeader.getAllHeadersList()).contains(entryNotInOriginalList);
    }

    @Test
    @SmallTest
    public void testHashCodeReturnsSameIntForEqualObjects() {
        FakeUrlResponse responseEqualToTest = mTestResponse.toBuilder().build();

        assertThat(mTestResponse.hashCode()).isEqualTo(mTestResponse.hashCode());
        assertThat(mTestResponse.hashCode()).isEqualTo(responseEqualToTest.hashCode());
        // Two non-equivalent values can map to the same hashCode.
    }

    @Test
    @SmallTest
    public void testToString() {
        String expectedString =
                "HTTP Status Code: "
                        + TEST_HTTP_STATUS_CODE
                        + " Headers: "
                        + mTestHeaders.toString()
                        + " Was Cached: "
                        + TEST_WAS_CACHED
                        + " Negotiated Protocol: "
                        + TEST_NEGOTIATED_PROTOCOL
                        + " Proxy Server: "
                        + TEST_PROXY_SERVER
                        + " Response Body (UTF-8): "
                        + TEST_BODY;
        String responseToString = mTestResponse.toString();

        assertThat(responseToString).isEqualTo(expectedString);
    }

    @Test
    @SmallTest
    public void testGetResponseWithUrlResponseInfo() {
        UrlResponseInfo info =
                new UrlResponseInfoImpl(
                        new ArrayList<>(),
                        TEST_HTTP_STATUS_CODE,
                        "",
                        mTestHeaders,
                        TEST_WAS_CACHED,
                        TEST_NEGOTIATED_PROTOCOL,
                        TEST_PROXY_SERVER,
                        0);
        FakeUrlResponse expectedResponse =
                new FakeUrlResponse.Builder()
                        .setHttpStatusCode(TEST_HTTP_STATUS_CODE)
                        .addHeader(TEST_HEADER_NAME, TEST_HEADER_VALUE)
                        .setWasCached(TEST_WAS_CACHED)
                        .setNegotiatedProtocol(TEST_NEGOTIATED_PROTOCOL)
                        .setProxyServer(TEST_PROXY_SERVER)
                        .build();

        FakeUrlResponse constructedResponse = new FakeUrlResponse(info);

        assertThat(constructedResponse).isEqualTo(expectedResponse);
    }

    @Test
    @SmallTest
    public void testGetResponesWithNullUrlResponseInfoGetsDefault() {
        // Set params that cannot be null in UrlResponseInfo in the expected response so that the
        // parameters found in the constructed response from UrlResponseInfo are the same
        // as the expected.
        FakeUrlResponse expectedResponse =
                new FakeUrlResponse.Builder()
                        .setHttpStatusCode(TEST_HTTP_STATUS_CODE)
                        .setWasCached(TEST_WAS_CACHED)
                        .addHeader(TEST_HEADER_NAME, TEST_HEADER_VALUE)
                        .build();
        // UnmodifiableList cannot be null.
        UrlResponseInfo info =
                new UrlResponseInfoImpl(
                        /* UrlChain */ new ArrayList<>(),
                        TEST_HTTP_STATUS_CODE,
                        null,
                        mTestHeaders,
                        TEST_WAS_CACHED,
                        null,
                        null,
                        0);

        FakeUrlResponse constructedResponse = new FakeUrlResponse(info);

        assertThat(constructedResponse).isEqualTo(expectedResponse);
    }

    @Test
    @SmallTest
    public void testInternalInitialHeadersListCantBeModified() {
        FakeUrlResponse defaultResponseWithHeader =
                new FakeUrlResponse.Builder()
                        .addHeader(TEST_HEADER_NAME, TEST_HEADER_VALUE)
                        .build();
        FakeUrlResponse defaultResponse = new FakeUrlResponse.Builder().build();

        assertThat(defaultResponseWithHeader.getAllHeadersList())
                .isNotEqualTo(defaultResponse.getAllHeadersList());
    }

    @Test
    @SmallTest
    public void testUrlResponseInfoHeadersMapIsCaseInsensitve() {
        UrlResponseInfo info =
                new UrlResponseInfoImpl(
                        new ArrayList<>(),
                        200,
                        "OK",
                        mTestResponse.getAllHeadersList(),
                        mTestResponse.getWasCached(),
                        mTestResponse.getNegotiatedProtocol(),
                        mTestResponse.getProxyServer(),
                        mTestResponse.getResponseBody().length);

        Map infoMap = info.getAllHeaders();

        assertThat(infoMap).containsKey(TEST_HEADER_NAME.toLowerCase(Locale.ROOT));
        assertThat(infoMap).containsKey(TEST_HEADER_NAME.toUpperCase(Locale.ROOT));
    }
}
