// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import org.chromium.base.BundleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;

/** CrashKey Recorder for installed modules. */
@NullMarked
class CrashKeyRecorder {
    public static void updateCrashKeys() {
        // Values with dots are interpreted as URLs. Config splits have dots in them. Make sure
        // they don't get sanitized.
        String value = BundleUtils.getInstalledSplitNamesForLogging().replace('.', '$');
        CrashKeys.getInstance().set(CrashKeyIndex.INSTALLED_MODULES, value);
    }
}
