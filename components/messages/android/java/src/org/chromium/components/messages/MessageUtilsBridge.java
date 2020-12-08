// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java counterpart to MessageUtilsBridge. Enables C++ feature code to know
 * if it is okay to show message ui.
 */
@JNINamespace("messages")
public class MessageUtilsBridge {
    @CalledByNative
    private static boolean isA11yEnabled() {
        return MessageUtils.isA11yEnabled();
    }
}
