// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.viz.service.gl;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;

abstract class ThrowUncaughtException {
    @CalledByNative
    private static void post() {
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                throw new RuntimeException("Intentional exception not caught by JNI");
            }
        });
    }
}
