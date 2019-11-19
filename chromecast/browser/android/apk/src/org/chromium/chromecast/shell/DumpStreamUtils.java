// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.charset.UnsupportedCharsetException;

/**
 * Utility class for dump streams
 *
 */
public final class DumpStreamUtils {
    private static final String TAG = "DumpStreamUtils";

    /**
     * Gets the first line from an input stream
     * Mutates given input stream.
     *
     * @return First line of the input stream.
     * @throws IOException
     */
    public static String getFirstLineFromStream(InputStream inputStream) throws IOException {
        try (InputStreamReader streamReader =
                        new InputStreamReader(inputStream, Charset.forName("UTF-8"));
                BufferedReader reader = new BufferedReader(streamReader)) {
            return reader.readLine();
        } catch (UnsupportedCharsetException e) {
            // Should never happen; all Java implementations are required to support UTF-8.
            Log.wtf(TAG, "UTF-8 not supported", e);
            return "";
        }
    }
}
