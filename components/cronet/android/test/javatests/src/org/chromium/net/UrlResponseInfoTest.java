// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.impl.UrlResponseInfoImpl;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Tests for {@link UrlResponseInfo}.
 */
@RunWith(AndroidJUnit4.class)
public class UrlResponseInfoTest {
    /**
     * Test for public API of {@link UrlResponseInfo}.
     */
    @Test
    @SmallTest
    public void testPublicAPI() throws Exception {
        final List<String> urlChain = new ArrayList<String>();
        urlChain.add("chromium.org");
        final int httpStatusCode = 200;
        final String httpStatusText = "OK";
        final Map.Entry<String, String> header =
                new AbstractMap.SimpleImmutableEntry<String, String>(
                        "Date", "Fri, 30 Oct 2015 14:26:41 GMT");
        final List<Map.Entry<String, String>> allHeadersList = new ArrayList<>();
        allHeadersList.add(header);
        final boolean wasCached = true;
        final String negotiatedProtocol = "quic/1+spdy/3";
        final String proxyServer = "example.com";
        final long receivedByteCount = 42;

        final UrlResponseInfo info =
                new UrlResponseInfoImpl(urlChain, httpStatusCode, httpStatusText, allHeadersList,
                        wasCached, negotiatedProtocol, proxyServer, receivedByteCount);
        assertThat(urlChain).isEqualTo(info.getUrlChain());
        assertThat(urlChain.get(urlChain.size() - 1)).isEqualTo(info.getUrl());
        try {
            info.getUrlChain().add("example.com");
            Assert.fail("getUrlChain() returned modifyable list.");
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
        assertThat(info.getHttpStatusCode()).isEqualTo(httpStatusCode);
        assertThat(info.getHttpStatusText()).isEqualTo(httpStatusText);
        assertThat(info.getAllHeadersAsList()).isEqualTo(allHeadersList);
        try {
            info.getAllHeadersAsList().add(
                    new AbstractMap.SimpleImmutableEntry<String, String>("X", "Y"));
            Assert.fail("getAllHeadersAsList() returned modifyable list.");
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
        assertThat(info.getAllHeaders())
                .containsExactly(header.getKey(), Arrays.asList(header.getValue()));
        assertThat(info.wasCached()).isEqualTo(wasCached);
        assertThat(info.getNegotiatedProtocol()).isEqualTo(negotiatedProtocol);
        assertThat(info.getProxyServer()).isEqualTo(proxyServer);
        assertThat(info.getReceivedByteCount()).isEqualTo(receivedByteCount);
    }
}