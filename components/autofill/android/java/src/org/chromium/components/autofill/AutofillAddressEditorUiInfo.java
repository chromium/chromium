// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

@JNINamespace("autofill")
@NullMarked
public class AutofillAddressEditorUiInfo {

    private final String mBestLanguageTag;
    private final List<AutofillAddressUiComponent> mComponents;

    @CalledByNative
    public AutofillAddressEditorUiInfo(
            @JniType("std::string") String bestLanguageTag,
            @JniType("std::vector<AutofillAddressUiComponentAndroid>")
                    List<AutofillAddressUiComponent> components) {
        mBestLanguageTag = bestLanguageTag;
        mComponents = components;
    }

    @CalledByNative
    public @JniType("std::string") String getBestLanguageTag() {
        return mBestLanguageTag;
    }

    @CalledByNative
    public @JniType("std::vector<AutofillAddressUiComponentAndroid>")
            List<AutofillAddressUiComponent> getComponents() {
        return mComponents;
    }
}
