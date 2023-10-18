// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.Arrays;
import java.util.List;

/**
 * The wrapper class of the native autofill::FormDataAndroid.
 */
@JNINamespace("autofill")
public class FormData {
    public final String mName;
    public final String mHost;
    public final List<FormFieldData> mFields;

    @CalledByNative
    private static FormData createFormData(String name, String origin, FormFieldData[] fields) {
        return new FormData(name, origin, Arrays.asList(fields));
    }

    public FormData(String name, String host, List<FormFieldData> fields) {
        mName = name;
        mHost = host;
        mFields = fields;
    }
}
