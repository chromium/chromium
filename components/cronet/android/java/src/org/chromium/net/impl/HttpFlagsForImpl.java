// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;

import org.chromium.net.httpflags.HttpFlagsLoader;
import org.chromium.net.httpflags.ResolvedFlags;
import org.chromium.net.impl.CronetLogger.CronetSource;

/**
 * Retrieves httpflags for the current Impl version.
 *
 * <p>Cronet is usually shipped as a package where the API and Impl are both built from the same
 * version. However, for Google Play Services channel, the API and Impl versions could diverge which
 * means that the cronet version used to fetch which flags applies changes correspondingly.
 */
public final class HttpFlagsForImpl {
    /**
     * Fetches and caches the available httpflags for the current impl depending on its version.
     *
     * <p>Never returns null: if HTTP flags were not loaded, will return an empty set of flags.
     */
    public static ResolvedFlags getHttpFlags(Context context, CronetSource source) {
        return HttpFlagsLoader.getHttpFlags(
                context,
                ImplVersion.getCronetVersion(),
                /* isLoadedFromApi= */ false,
                CronetManifest.isAppOptedInForTelemetry(context, source));
    }
}
