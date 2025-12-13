// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.net.Uri;

import org.chromium.build.annotations.NullMarked;

/**
 * A utility class to handle parsing of chrome-extension:// URLs in Java.
 *
 * <p>An extension's origin is defined as `chrome-extension://<extension_id>`. This utility provides
 * a canonical way to extract that origin, ensuring that other parts of the URL (like paths or query
 * parameters) are ignored.
 */
@NullMarked
public final class ExtensionUrlUtil {
    private ExtensionUrlUtil() {}

    /**
     * Extracts the origin from a chrome-extension URL string.
     *
     * @param url The URL string to parse.
     * @return The canonical origin string (e.g., "chrome-extension://<id>").
     * @throws IllegalArgumentException if the URL is not a valid chrome-extension URL.
     */
    public static String getOrigin(String url) {
        if (url == null) {
            throw new IllegalArgumentException("Cannot extract origin from null URL.");
        }
        return getOrigin(Uri.parse(url));
    }

    /**
     * Extracts the origin from a chrome-extension URI.
     *
     * @param uri The URI to parse.
     * @return The canonical origin string (e.g., "chrome-extension://<id>").
     * @throws IllegalArgumentException if the URI is not a valid chrome-extension URL.
     */
    public static String getOrigin(Uri uri) {
        if (uri == null) {
            throw new IllegalArgumentException("Cannot extract origin from null URI.");
        }
        if (!UrlConstants.CHROME_EXTENSION_SCHEME.equals(uri.getScheme())) {
            throw new IllegalArgumentException(
                    "URI scheme is not " + UrlConstants.CHROME_EXTENSION_SCHEME);
        }
        String host = uri.getHost();
        if (host == null || host.isEmpty()) {
            throw new IllegalArgumentException(
                    "Extension URL must have a host (the extension ID).");
        }
        return UrlConstants.CHROME_EXTENSION_URL_PREFIX + host;
    }

    /** Returns true if the URL is a chrome extension URL; false otherwise. */
    public static boolean isExtensionUrl(String url) {
        if (url == null) {
            return false;
        }
        return url.startsWith(UrlConstants.CHROME_EXTENSION_URL_PREFIX);
    }
}
