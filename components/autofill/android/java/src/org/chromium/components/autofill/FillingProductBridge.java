// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

@JNINamespace("autofill")
@NullMarked
public class FillingProductBridge {

    public static @FillingProduct int getFillingProductFromSuggestionType(
            @SuggestionType int type) {
        return FillingProductBridgeJni.get().getFillingProductFromSuggestionType(type);
    }

    @NativeMethods
    public interface Natives {
        @FillingProduct
        int getFillingProductFromSuggestionType(@SuggestionType int type);
    }
}
