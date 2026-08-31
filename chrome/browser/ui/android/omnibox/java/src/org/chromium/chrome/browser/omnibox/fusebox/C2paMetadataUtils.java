// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.ContentResolver;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/**
 * Utility class for inspecting image metadata, specifically checking for C2PA provenance headers.
 */
@NullMarked
public final class C2paMetadataUtils {
    private static final String TAG = "C2paMetadataUtils";
    @VisibleForTesting static final int MAX_C2PA_SEARCH_BYTES = 256 * 1024;
    @VisibleForTesting static final int C2PA_CHUNK_BUFFER_SIZE = 8192;
    @VisibleForTesting static final String C2PA_MARKER = "urn:c2pa:";
    @VisibleForTesting static final int C2PA_OVERLAP_SIZE = C2PA_MARKER.length() - 1;

    private C2paMetadataUtils() {}

    /**
     * Checks if the image at the given URI contains C2PA metadata in the first 256KB of the stream.
     * Matches the C2PA detection algorithm in ComposeboxQueryController::HasC2paMetadata.
     *
     * @param contentResolver The ContentResolver used to open the URI stream.
     * @param uri The URI of the image to check.
     * @return true if C2PA metadata is detected and the bypass feature is enabled; false otherwise.
     */
    // TODO(crbug.com/555150730): Pass the c2pa header detection bit from Java to c++ so that c2pa
    // header detection can be skipped in the c++ layer.
    public static boolean hasC2paMetadata(ContentResolver contentResolver, Uri uri) {
        if (!ChromeFeatureList.sLensBypassCompressionForC2pa.isEnabled()) {
            return false;
        }
        try (InputStream inputStream = contentResolver.openInputStream(uri)) {
            if (inputStream == null) return false;
            return hasC2paMetadata(inputStream);
        } catch (IOException e) {
            Log.w(TAG, "Failed to read attachment for C2PA check", e);
            return false;
        }
    }

    /**
     * Checks if the given InputStream contains C2PA metadata in the first 256KB.
     *
     * @param inputStream The stream to scan.
     * @return true if C2PA metadata is detected; false otherwise.
     */
    public static boolean hasC2paMetadata(InputStream inputStream) throws IOException {
        byte[] buffer = new byte[C2PA_CHUNK_BUFFER_SIZE + C2PA_OVERLAP_SIZE];
        int overlapCount = 0;
        int totalBytesRead = 0;

        while (totalBytesRead < MAX_C2PA_SEARCH_BYTES) {
            int bytesToRead =
                    Math.min(C2PA_CHUNK_BUFFER_SIZE, MAX_C2PA_SEARCH_BYTES - totalBytesRead);
            int read = inputStream.read(buffer, overlapCount, bytesToRead);
            if (read == -1) break;

            int validLength = overlapCount + read;
            totalBytesRead += read;

            // TODO(crbug.com/555150321): Improve c2pa detection heuristic.
            if (new String(buffer, 0, validLength, StandardCharsets.US_ASCII)
                    .contains(C2PA_MARKER)) {
                return true;
            }

            overlapCount = Math.min(validLength, C2PA_OVERLAP_SIZE);
            System.arraycopy(buffer, validLength - overlapCount, buffer, 0, overlapCount);
        }
        return false;
    }
}
