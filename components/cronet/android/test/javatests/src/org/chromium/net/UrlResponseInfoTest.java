// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.impl.UrlResponseInfoImpl;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/** Tests for {@link UrlResponseInfo}. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
public class UrlResponseInfoTest {
    /** Test for public API of {@link UrlResponseInfo}. */
    @Test
    @SmallTest
    public void testPublicAPI() {
        final List<String> urlChain = new ArrayList<>();
        urlChain.add("chromium.org");
        final int httpStatusCode = 200;
        final String httpStatusText = "OK";
        final Map.Entry<String, String> header =
                new AbstractMap.SimpleImmutableEntry<>("Date", "Fri, 30 Oct 2015 14:26:41 GMT");
        final List<Map.Entry<String, String>> allHeadersList = new ArrayList<>();
        allHeadersList.add(header);
        final boolean wasCached = true;
        final String negotiatedProtocol = "quic/1+spdy/3";
        final String proxyServer = "example.com";
        final long receivedByteCount = 42;

        final UrlResponseInfo info =
                new UrlResponseInfoImpl(
                        urlChain,
                        httpStatusCode,
                        httpStatusText,
                        allHeadersList,
                        wasCached,
                        negotiatedProtocol,
                        proxyServer,
                        receivedByteCount);

        assertThat(info).hasUrlChainThat().isEqualTo(urlChain);
        assertThat(info).hasUrlThat().isEqualTo(urlChain.get(urlChain.size() - 1));
        assertThrows(
                UnsupportedOperationException.class, () -> info.getUrlChain().add("example.com"));
        assertThat(info).hasHttpStatusCodeThat().isEqualTo(httpStatusCode);
        assertThat(info).hasHttpStatusTextThat().isEqualTo(httpStatusText);
        assertThat(info).hasHeadersListThat().isEqualTo(allHeadersList);
        assertThrows(
                UnsupportedOperationException.class,
                () ->
                        info.getAllHeadersAsList()
                                .add(new AbstractMap.SimpleImmutableEntry<>("X", "Y")));
        assertThat(info)
                .hasHeadersThat()
                .containsExactly(header.getKey(), Arrays.asList(header.getValue()));
        assertThat(info).wasCached();
        assertThat(info).hasNegotiatedProtocolThat().isEqualTo(negotiatedProtocol);
        assertThat(info).hasProxyServerThat().isEqualTo(proxyServer);
        assertThat(info).hasReceivedByteCountThat().isEqualTo(receivedByteCount);
    }
}
