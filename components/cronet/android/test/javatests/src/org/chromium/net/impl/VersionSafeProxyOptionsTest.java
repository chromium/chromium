// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.Proxy;
import org.chromium.net.ProxyOptions;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executors;
import java.util.stream.Collectors;

/** Test version safe handling of ProxyOptions. */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
@IgnoreFor(
        implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
        reason =
                "These tests are implementation independent: we use CronetTestRule only to access"
                        + " RequiresMinAndroidApi.")
public class VersionSafeProxyOptionsTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    @Test
    @SmallTest
    public void testDirectProxy_correctlyCreatesProxyCallback() {
        ProxyOptions proxyOptions = new ProxyOptions(Arrays.asList((Proxy) null));
        VersionSafeProxyOptions safeProxyOptions = new VersionSafeProxyOptions(proxyOptions);
        List<VersionSafeProxyCallback> safeProxyCallbacks =
                safeProxyOptions.createProxyCallbackList();
        assertThat(safeProxyCallbacks).isNotNull();
        assertThat(safeProxyCallbacks).containsExactly((VersionSafeProxyCallback) null);
    }

    @Test
    @SmallTest
    public void testDirectProxy_correctlyCreatesProxyOptionsProto() {
        ProxyOptions proxyOptions = new ProxyOptions(Arrays.asList((Proxy) null));
        VersionSafeProxyOptions safeProxyOptions = new VersionSafeProxyOptions(proxyOptions);
        org.chromium.net.impl.proto.ProxyOptions proxyOptionsProto =
                safeProxyOptions.createProxyOptionsProto();
        assertThat(proxyOptionsProto).isNotNull();
        assertThat(proxyOptionsProto.getProxiesCount()).isEqualTo(1);
        org.chromium.net.impl.proto.Proxy proxyProto = proxyOptionsProto.getProxies(0);
        assertThat(proxyProto.getScheme())
                .isEqualTo(org.chromium.net.impl.proto.ProxyScheme.DIRECT);
    }

    @Test
    @SmallTest
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testHttpProxy_correctlyCreatesProxyOptionsProto() {
        ProxyOptions proxyOptions =
                new ProxyOptions(
                        Arrays.asList(
                                new Proxy(
                                        Proxy.SCHEME_HTTP,
                                        "not-existing-hostname",
                                        8080,
                                        Executors.newSingleThreadExecutor(),
                                        Mockito.mock(Proxy.HttpConnectCallback.class))));
        VersionSafeProxyOptions safeProxyOptions = new VersionSafeProxyOptions(proxyOptions);
        org.chromium.net.impl.proto.ProxyOptions proxyOptionsProto =
                safeProxyOptions.createProxyOptionsProto();
        assertThat(proxyOptionsProto).isNotNull();
        assertThat(proxyOptionsProto.getProxiesCount()).isEqualTo(1);
        org.chromium.net.impl.proto.Proxy proxyProto = proxyOptionsProto.getProxies(0);
        assertThat(proxyProto.getScheme()).isEqualTo(org.chromium.net.impl.proto.ProxyScheme.HTTP);
        assertThat(proxyProto.getHost()).isEqualTo("not-existing-hostname");
        assertThat(proxyProto.getPort()).isEqualTo(8080);
    }

    @Test
    @SmallTest
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testHttpProxy_correctlyCreatesProxyCallback() {
        ProxyOptions proxyOptions =
                new ProxyOptions(
                        Arrays.asList(
                                new Proxy(
                                        Proxy.SCHEME_HTTP,
                                        "not-existing-hostname",
                                        8080,
                                        Executors.newSingleThreadExecutor(),
                                        Mockito.mock(Proxy.HttpConnectCallback.class))));
        VersionSafeProxyOptions safeProxyOptions = new VersionSafeProxyOptions(proxyOptions);
        List<VersionSafeProxyCallback> safeProxyCallbacks =
                safeProxyOptions.createProxyCallbackList();
        assertThat(safeProxyCallbacks).isNotNull();
        assertThat(safeProxyCallbacks).hasSize(1);
        assertThat(safeProxyCallbacks.get(0)).isNotNull();
    }

    @Test
    @SmallTest
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testHttpsProxy_correctlyCreatesProxyOptionsProto() {
        ProxyOptions proxyOptions =
                new ProxyOptions(
                        Arrays.asList(
                                new Proxy(
                                        Proxy.SCHEME_HTTPS,
                                        "not-existing-hostname",
                                        8080,
                                        Executors.newSingleThreadExecutor(),
                                        Mockito.mock(Proxy.HttpConnectCallback.class))));
        VersionSafeProxyOptions safeProxyOptions = new VersionSafeProxyOptions(proxyOptions);
        org.chromium.net.impl.proto.ProxyOptions proxyOptionsProto =
                safeProxyOptions.createProxyOptionsProto();
        assertThat(proxyOptionsProto).isNotNull();
        assertThat(proxyOptionsProto.getProxiesCount()).isEqualTo(1);
        org.chromium.net.impl.proto.Proxy proxyProto = proxyOptionsProto.getProxies(0);
        assertThat(proxyProto.getScheme()).isEqualTo(org.chromium.net.impl.proto.ProxyScheme.HTTPS);
        assertThat(proxyProto.getHost()).isEqualTo("not-existing-hostname");
        assertThat(proxyProto.getPort()).isEqualTo(8080);
    }

    @Test
    @SmallTest
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testHttpsProxy_correctlyCreatesProxyCallback() {
        ProxyOptions proxyOptions =
                new ProxyOptions(
                        Arrays.asList(
                                new Proxy(
                                        Proxy.SCHEME_HTTPS,
                                        "not-existing-hostname",
                                        8080,
                                        Executors.newSingleThreadExecutor(),
                                        Mockito.mock(Proxy.HttpConnectCallback.class))));
        VersionSafeProxyOptions safeProxyOptions = new VersionSafeProxyOptions(proxyOptions);
        List<VersionSafeProxyCallback> safeProxyCallbacks =
                safeProxyOptions.createProxyCallbackList();
        assertThat(safeProxyCallbacks).isNotNull();
        assertThat(safeProxyCallbacks).hasSize(1);
        assertThat(safeProxyCallbacks.get(0)).isNotNull();
    }

    @Test
    @SmallTest
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testListWithMultipleProxies() {
        Proxy.HttpConnectCallback httpsProxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class);
        Proxy.HttpConnectCallback httpProxyCallback = Mockito.mock(Proxy.HttpConnectCallback.class);
        ProxyOptions proxyOptions =
                new ProxyOptions(
                        Arrays.asList(
                                new Proxy(
                                        Proxy.SCHEME_HTTPS,
                                        "not-existing-hostname",
                                        8080,
                                        Executors.newSingleThreadExecutor(),
                                        httpsProxyCallback),
                                new Proxy(
                                        Proxy.SCHEME_HTTP,
                                        "not-existing-hostname",
                                        8080,
                                        Executors.newSingleThreadExecutor(),
                                        httpProxyCallback),
                                null));
        VersionSafeProxyOptions safeProxyOptions = new VersionSafeProxyOptions(proxyOptions);
        org.chromium.net.impl.proto.ProxyOptions proxyOptionsProto =
                safeProxyOptions.createProxyOptionsProto();
        assertThat(proxyOptionsProto).isNotNull();
        assertThat(proxyOptionsProto.getProxiesCount()).isEqualTo(3);
        // Confirm that the original order within ProxyOptions#getProxyList is maintained for
        // Proxy's proto.
        assertThat(
                        proxyOptionsProto.getProxiesList().stream()
                                .map(p -> p.getScheme())
                                .collect(Collectors.toList()))
                .containsExactly(
                        org.chromium.net.impl.proto.ProxyScheme.HTTPS,
                        org.chromium.net.impl.proto.ProxyScheme.HTTP,
                        org.chromium.net.impl.proto.ProxyScheme.DIRECT)
                .inOrder();
        // Confirm that the original order within ProxyOptions#getProxyList is maintained for
        // Callback's proto.
        List<VersionSafeProxyCallback> safeProxyCallbacks =
                safeProxyOptions.createProxyCallbackList();
        assertThat(safeProxyCallbacks).isNotNull();
        assertThat(safeProxyCallbacks).hasSize(3);
        // Verify the order by verifying that we're calling the right mock.
        safeProxyCallbacks.get(0).onBeforeTunnelRequest(any());
        Mockito.verify(httpsProxyCallback, times(1)).onBeforeRequest(any());
        Mockito.verify(httpProxyCallback, never()).onBeforeRequest(any());
        assertThat(safeProxyCallbacks.get(2)).isNull();
        safeProxyCallbacks.get(1).onBeforeTunnelRequest(any());
        Mockito.verify(httpProxyCallback, times(1)).onBeforeRequest(any());
    }
}
