// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.Arrays;
import java.util.List;

/**
 * The wrapper class of the native autofill::FormDataAndroid.
 */
@JNINamespace("autofill")
public class FormData {
    public final int mSessionId;
    public final String mName;
    public final String mHost;
    public final List<FormFieldData> mFields;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    static FormData createFormData(
            int sessionId, String name, String origin, FormFieldData[] fields) {
        return new FormData(sessionId, name, origin, Arrays.asList(fields));
    }

    public FormData(int sessionId, String name, String host, List<FormFieldData> fields) {
        mSessionId = sessionId;
        mName = name;
        mHost = host;
        mFields = fields;
    }
}
