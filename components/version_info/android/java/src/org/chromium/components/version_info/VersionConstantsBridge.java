// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.version_info;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.build.annotations.MainDex;

/**
 * Bridge between native and VersionConstants.java.
 */
@MainDex
public class VersionConstantsBridge {
    @CalledByNative
    public static int getChannel() {
        return VersionConstants.CHANNEL;
    }
}
