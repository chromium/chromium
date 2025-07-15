// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.chromium.net.httpflags.HttpFlagsLoader;
import org.chromium.net.httpflags.ResolvedFlags;
import org.chromium.net.impl.CronetManifest;

/**
 * Retrieves httpflags for the current API version.
 *
 * <p>Cronet is usually shipped as a package where the API and Impl are both built from the same
 * version. However, for Google Play Services channel, the API and Impl versions could diverge which
 * means that the cronet version used to fetch which flags applies changes correspondingly.
 */
final class HttpFlagsForApi {
    /**
     * Fetches and caches the available httpflags for the current API version.
     *
     * <p>Never returns null: if HTTP flags were not loaded, will return an empty set of flags.
     */
    public static ResolvedFlags getHttpFlags(Context context) {
        return HttpFlagsLoader.getHttpFlags(
                context,
                ApiVersion.getCronetVersion(),
                /* isLoadedFromApi= */ true,
                CronetManifest.isAppOptedInForTelemetry(context));
    }
}
