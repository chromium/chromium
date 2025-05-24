// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode.test.disk_write_proxy;

import org.chromium.components.strictmode.test.disk_write_helper.DiskWriteHelper;

/** Calls {@link DiskWriteHelper} */
public class DiskWriteProxy {
    public static void callDiskWriteHelper() {
        DiskWriteHelper.doDiskWrite();
    }
}
