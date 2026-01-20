// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresOptIn;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Defines a proxy configuration that can be used by {@link CronetEngine}. */
public final class ProxyOptions {

    /**Defines behavior after all proxies in a {@link ProxyOptions} have failed. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
            value = {
                ALL_PROXIES_FAILED_BEHAVIOR_DISALLOW_DIRECT,
                ALL_PROXIES_FAILED_BEHAVIOR_ALLOW_DIRECT
            })
    public @interface AllProxiesFailedBehavior {}

    /**
     * Disallows direct traffic.
     *
     * <p>This defines a fail-closed behavior: if all proxies in a {@link ProxyOptions} have failed,
     * CronetEngine won't fall back onto non-proxied connections.
     */
    public static final int ALL_PROXIES_FAILED_BEHAVIOR_DISALLOW_DIRECT = 0;

    /**
     * Allows direct, non-proxied, traffic.
     *
     * <p>This defines a fail-open behavior: if all proxies in a {@link ProxyOptions} have failed,
     * CronetEngine will fall back onto non-proxied connections.
     */
    public static final int ALL_PROXIES_FAILED_BEHAVIOR_ALLOW_DIRECT = 1;

    /**
     * Constructs a proxy configuration out of a list of {@link Proxy}.
     *
     * <p>Proxies in the list will be used in order. Proxy in position N+1 will be used only if
     * CronetEngine failed to use proxy in position N. If proxy in position N fails, for any reason
     * (including tunnel closures triggered via {@link Proxy.HttpConnectCallback}), but proxy in
     * position N+1 succeeds, proxies in position N will be temporarily deprioritized. While a proxy
     * is deprioritized it will be used only as a last resort.
     *
     * @param proxyList The list of {@link Proxy} that defines this configuration.
     * @param allProxiesFailedBehavior How CronetEngine must behave after it has failed to use all
     *     proxies in {@code proxyList}.
     * @throws IllegalArgumentException If the proxy list is empty, or any element is {@code null}.
     */
    @NonNull
    public static ProxyOptions fromProxyList(
            @NonNull List<Proxy> proxyList,
            @AllProxiesFailedBehavior int allProxiesFailedBehavior) {
        if (Objects.requireNonNull(proxyList).isEmpty()) {
            throw new IllegalArgumentException("proxyList cannot be empty");
        }
        if (proxyList.contains(null)) {
            throw new IllegalArgumentException("proxyList cannot contain null");
        }
        var proxyListCopy = new ArrayList<>(proxyList);
        switch (allProxiesFailedBehavior) {
            case ALL_PROXIES_FAILED_BEHAVIOR_DISALLOW_DIRECT -> {}
            case ALL_PROXIES_FAILED_BEHAVIOR_ALLOW_DIRECT -> proxyListCopy.add(null);
        }
        return new ProxyOptions(proxyListCopy);
    }

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
     *
     * @deprecated Use {@link #fromProxyList(List<Proxy>, int)} instead.
     */
    @Deprecated
    @NonNull
    public static ProxyOptions fromProxyList(@NonNull List<Proxy> proxyList) {
        return new ProxyOptions(new ArrayList<>(Objects.requireNonNull(proxyList)));
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

    @NonNull List<Proxy> getProxyList() {
        return mProxyList;
    }

    private ProxyOptions(@NonNull List<Proxy> proxyList) {
        if (Objects.requireNonNull(proxyList).isEmpty()) {
            throw new IllegalArgumentException("ProxyList cannot be empty");
        }
        int nullElemPos = proxyList.indexOf(null);
        if (nullElemPos != -1 && nullElemPos != proxyList.size() - 1) {
            throw new IllegalArgumentException(
                    "Null is allowed only as the last element in the proxy list");
        }

        mProxyList = Collections.unmodifiableList(proxyList);
    }

    private final @NonNull List<Proxy> mProxyList;
}
