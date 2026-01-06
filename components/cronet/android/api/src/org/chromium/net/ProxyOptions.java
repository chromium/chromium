// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresOptIn;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Defines a proxy configuration that can be used by {@link CronetEngine}. */
public final class ProxyOptions {

    /**
     * Constructs a proxy configuration out of a list of {@link Proxy}.
     *
     * <p>Proxies in the list will be used in order. Proxy in position N+1 will be used only if
     * CronetEngine failed to use proxy in position N. If proxy in position N fails, for any reason
     * (including tunnel closures triggered via {@link Proxy.HttpConnectCallback}), but proxy in
     * position N+1 succeeds, proxies in position N will be temporarily deprioritized. While a proxy
     * is deprioritized it will be used only as a last resort.
     *
     * <p>A {@code null} list element represents a non-proxied connection, in which case requests
     * will be sent directly to their destination, without going through a tunnel. {@code null} is
     * only allowed as the last element in the list. This can be used to define a fail-open or
     * fail-closed behavior: if the all of the proxies specified in the list happen to fail, adding
     * (or not adding) a {@code null} element at the end of the list controls whether falling back
     * onto non-proxied connections is allowed.
     *
     * @param proxyList The list of {@link Proxy} that defines this configuration.
     * @throws IllegalArgumentException If the proxy list is empty; or, an element, other than the
     *     last one in the list, is {@code null}.
     */
    @NonNull
    public static ProxyOptions fromProxyList(@NonNull List<Proxy> proxyList) {
        return new ProxyOptions(proxyList);
    }

    /**
     * Constructs a proxy configuration out of a list of {@link Proxy}.
     *
     * <p>Proxies in the list will be used in order. Proxy in position N+1 will be used only if we
     * failed to use proxy in position N. If proxy in position N fails for any reason (including
     * cancellations triggered via {@link Proxy.Callback}), but proxy in position N+1 succeeds,
     * proxies in position N will be temporarily deprioritized. While a proxy is deprioritized it
     * used only as a last resort.
     *
     * <p>A {@code null} list element represents a non-proxied connection, in which case requests
     * will be sent directly to the destination. It is only allowed as the last element in the list.
     * This can be used to define fail-open/fail-closed semantics: if the all of the proxies
     * specified in the list happen to fail, adding (or not adding) a {@code null} element at the
     * end of the list will control whether non-proxied connections are allowed.
     *
     * @param proxyList The list of {@link Proxy} that defines this configuration.
     * @throws IllegalArgumentException If the proxy list is empty; or, an element, other than the
     *     last one in the list, is {@code null}.
     * @deprecated Call {@link fromProxyList} instead.
     */
    @Deprecated
    public ProxyOptions(@NonNull List<Proxy> proxyList) {
        if (Objects.requireNonNull(proxyList).isEmpty()) {
            throw new IllegalArgumentException("ProxyList cannot be empty");
        }
        int nullElemPos = proxyList.indexOf(null);
        if (nullElemPos != -1 && nullElemPos != proxyList.size() - 1) {
            throw new IllegalArgumentException(
                    "Null is allowed only as the last element in the proxy list");
        }
        this.mProxyList = Collections.unmodifiableList(new ArrayList<>(proxyList));
    }

    /**
     * Returns the list of proxies that are part of this proxy configuration.
     *
     * @deprecated This will be made package private before Cronet proxy APIs are made
     *     non-experimental.
     */
    @Deprecated
    public @NonNull List<Proxy> getProxyList() {
        return mProxyList;
    }

    /**
     * An annotation for APIs which are not considered stable yet.
     *
     * <p>Experimental APIs are subject to change, breakage, or removal at any time and may not be
     * production ready.
     *
     * <p>It's highly recommended to reach out to Cronet maintainers (<code>net-dev@chromium.org
     * </code>) before using one of the APIs annotated as experimental outside of debugging and
     * proof-of-concept code.
     *
     * <p>By using an Experimental API, applications acknowledge that they are doing so at their own
     * risk.
     */
    @RequiresOptIn
    public @interface Experimental {}

    private final @NonNull List<Proxy> mProxyList;
}
