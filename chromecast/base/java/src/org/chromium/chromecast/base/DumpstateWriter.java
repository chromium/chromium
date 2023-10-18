// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class for storing values to be written when a bugreport is taken.
 */
@JNINamespace("chromecast")
public final class DumpstateWriter {
    private static final String TAG = "DumpstateWriter";

    private static DumpstateWriter sDumpstateWriter;

    private Map<String, String> mDumpValues;

    public DumpstateWriter() {
        sDumpstateWriter = this;
        mDumpValues = new HashMap<>();
    }

    public void writeDumpValues(PrintWriter writer) {
        for (Map.Entry<String, String> entry : mDumpValues.entrySet()) {
            writer.println(entry.getKey() + ": " + entry.getValue());
        }
    }

    @CalledByNative
    private static void addDumpValue(String name, String value) {
        if (sDumpstateWriter == null) {
            Log.w(TAG, "DumpstateWriter must be created before adding values: %s: %s", name, value);
            return;
        }
        sDumpstateWriter.mDumpValues.put(name, value);
    }
}
