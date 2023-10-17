// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.aboutui;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Credits-related utilities. */
@JNINamespace("about_ui")
public class CreditUtils {
    private CreditUtils() {}

    @NativeMethods
    public interface Natives {
        /** Writes the chrome://credits HTML to the given descriptor. */
        void writeCreditsHtml(int fd);
    }
}
