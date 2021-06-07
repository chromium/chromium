// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;

/**
 * The wrap class of native autofill::FormDataAndroid.
 */
@JNINamespace("autofill")
public class FormData {
    public final String mName;
    public final String mHost;
    public final ArrayList<FormFieldData> mFields;

    @CalledByNative
    private static FormData createFormData(
            long nativeObj, String name, String origin, int fieldCount) {
        return new FormData(nativeObj, name, origin, fieldCount);
    }

    private static ArrayList<FormFieldData> popupFormFields(long nativeObj, int fieldCount) {
        FormFieldData formFieldData = FormDataJni.get().getNextFormFieldData(nativeObj);
        ArrayList<FormFieldData> fields = new ArrayList<FormFieldData>(fieldCount);
        while (formFieldData != null) {
            fields.add(formFieldData);
            formFieldData = FormDataJni.get().getNextFormFieldData(nativeObj);
        }
        assert fields.size() == fieldCount;
        return fields;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public FormData(String name, String host, ArrayList<FormFieldData> fields) {
        mName = name;
        mHost = host;
        mFields = fields;
    }

    private FormData(long nativeObj, String name, String host, int fieldCount) {
        this(name, host, popupFormFields(nativeObj, fieldCount));
    }

    @NativeMethods
    interface Natives {
        FormFieldData getNextFormFieldData(long nativeFormDataAndroid);
    }
}
