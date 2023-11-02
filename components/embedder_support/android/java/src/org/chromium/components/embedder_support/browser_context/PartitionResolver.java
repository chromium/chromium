// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.browser_context;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Interface to abstract embedder specific partition serialization logic. Partitions being the keys
 * used to separate browser instances from each other. Implementations should be capable of
 * resolving tokens generated from one type of partition into another. For example, the token
 * from {@link #tokenize(BrowserContextHandle)} may be passed into
 * {@link #resolveSimpleFactoryKey(String)}. If a token is serialized to durable storage, it is also
 * possible the token was generated from a previous version of the client, and ideally resolution
 * should be backwards compatible. Tokenizing the same partition should result in identical token
 * strings for the current execution of the process, but may change upon upgrade. Callers should be
 * careful when their tokens could be from older versions.
 */
public interface PartitionResolver {
    /**
     * Creates a serialized token that can be used to look up the given browser key later.
     * @param handle The reference to the current browser instance.
     * @return A serialized token corresponding to the given handle, or an empty string if handle is
     *         invalid.
     */
    @Nullable
    String tokenize(BrowserContextHandle handle);

    /**
     * Creates a serialized token that can be used to look up the given browser key later.
     * @param handle The reference to the current browser instance.
     * @return A serialized token corresponding to the given handle, or an empty string if handle is
     *         invalid
     */
    @Nullable
    String tokenize(SimpleFactoryKeyHandle handle);

    /**
     * Resolves a token to a browser context. If resolution fails, null will be returned. Callers
     * should be careful to be able to handle both inline/renterant and asynchronous invocations of
     * the passed callback.
     * @param token A value previously provided by a tokenize call.
     * @param callback A callback to pass the resolved browser context on success, otherwise null.
     */
    void resolveBrowserContext(String token, Callback<BrowserContextHandle> callback);

    /**
     * Resolves a token to a simple key. This should be callable during reduced/service mode.
     * Callers should be careful to be able to handle both inline/renterant and asynchronous
     * invocations of the passed callback.
     * @param token A value previously provided by a tokenize call.
     * @param callback A callback to pass the resolved simple factory key on success, otherwise
     *        null.
     */
    void resolveSimpleFactoryKey(String token, Callback<SimpleFactoryKeyHandle> callback);
}
