// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.FieldType;

import java.util.Objects;

/** Represents information of an Autofill AI attribute type. */
@JNINamespace("autofill")
@NullMarked
public class AttributeType {
    // This maps to a C++ enum which defines the name/type of the attribute.
    private final @AttributeTypeName int mTypeName;
    // The name of the attribute as displayed to the user.
    private final String mTypeNameAsString;
    // The `dataType` defined whether the input rendered for the attribute should be a country
    // selector, a date picker or a simple text field.
    private final @DataType int mDataType;
    // The field type this attribute type describes.
    private final @FieldType int mFieldType;

    @CalledByNative
    public AttributeType(
            @AttributeTypeName int typeName,
            @JniType("std::u16string") String typeNameAsString,
            @DataType int dataType,
            @FieldType int fieldType) {
        mTypeName = typeName;
        mTypeNameAsString = typeNameAsString;
        mDataType = dataType;
        mFieldType = fieldType;
    }

    @CalledByNative
    public @AttributeTypeName int getTypeName() {
        return mTypeName;
    }

    public String getTypeNameAsString() {
        return mTypeNameAsString;
    }

    public @DataType int getDataType() {
        return mDataType;
    }

    public @FieldType int getFieldType() {
        return mFieldType;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof AttributeType that) {
            return mTypeName == that.mTypeName;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTypeName);
    }
}
