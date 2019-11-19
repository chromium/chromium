// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.aboutui;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

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
