// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.net.Uri;

/**
 * Helper class for providering some helper method needed by DownloadCollectionBridge.
 */
public class DownloadDelegate {
    public DownloadDelegate() {}

    /**
     * If the given MIME type is null, or one of the "generic" types (text/plain
     * or application/octet-stream) map it to a type that Android can deal with.
     * If the given type is not generic, return it unchanged.
     *
     * @param mimeType MIME type provided by the server.
     * @param url URL of the data being loaded.
     * @param filename file name obtained from content disposition header
     * @return The MIME type that should be used for this data.
     */
    public String remapGenericMimeType(String mimeType, String url, String filename) {
        return mimeType;
    }

    /**
     * Parses an originating URL string and returns a valid Uri that can be inserted into
     * DownloadManager. The returned Uri has to be null or non-empty http(s) scheme.
     * @param originalUrl String representation of the originating URL.
     * @return A valid Uri that can be accepted by DownloadManager.
     */
    public Uri parseOriginalUrl(String originalUrl) {
        return Uri.parse(originalUrl);
    }

    /**
     * Returns whether the downloaded file path is on an external SD card.
     * @param filePath The download file path.
     * @return Whether download is on external sd card.
     */
    public boolean isDownloadOnSDCard(String filePath) {
        return false;
    }
}
