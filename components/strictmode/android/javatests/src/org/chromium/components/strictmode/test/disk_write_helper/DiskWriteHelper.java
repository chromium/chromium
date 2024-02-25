// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode.test.disk_write_helper;

import androidx.core.content.ContextCompat;
import androidx.test.InstrumentationRegistry;

import java.io.File;
import java.io.FileOutputStream;

/* Utility for triggering a StrictMode violation. */
public class DiskWriteHelper {
    public static void doDiskWrite() {
        File dataDir = ContextCompat.getDataDir(InstrumentationRegistry.getTargetContext());
        File outFile = new File(dataDir, "random.txt");
        try (FileOutputStream out = new FileOutputStream(outFile)) {
            out.write(1);
        } catch (Exception e) {
        }
    }
}
