// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

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

    private long mNativeObj;

    @CalledByNative
    private static FormData createFormData(
            long nativeObj, String name, String origin, int fieldCount) {
        return new FormData(nativeObj, name, origin, fieldCount);
    }

    private FormData(long nativeObj, String name, String host, int fieldCount) {
        mNativeObj = nativeObj;
        mName = name;
        mHost = host;
        mFields = new ArrayList<FormFieldData>(fieldCount);
        popupFormFields(fieldCount);
    }

    private void popupFormFields(int fieldCount) {
        FormFieldData formFieldData =
                FormDataJni.get().getNextFormFieldData(mNativeObj, FormData.this);
        while (formFieldData != null) {
            mFields.add(formFieldData);
            formFieldData = FormDataJni.get().getNextFormFieldData(mNativeObj, FormData.this);
        }
        assert mFields.size() == fieldCount;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeObj = 0;
    }

    @NativeMethods
    interface Natives {
        FormFieldData getNextFormFieldData(long nativeFormDataAndroid, FormData caller);
    }
}
