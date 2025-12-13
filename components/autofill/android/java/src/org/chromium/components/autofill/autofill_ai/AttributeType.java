// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Represents information of an Autofill AI attribute type. */
@JNINamespace("autofill")
@NullMarked
public class AttributeType {
    // This maps to a C++ enum which defines the name/type of the attribute.
    public final @AttributeTypeName int typeName;
    // The name of the attribute as displayed to the user.
    public final String typeNameAsString;
    // The `dataType` defined whether the input rendered for the attribute should be a country
    // selector, a date picker or a simple text field.
    public final @DataType int dataType;

    @CalledByNative
    public AttributeType(
            @AttributeTypeName int typeName,
            @JniType("std::u16string") String typeNameAsString,
            @DataType int dataType) {
        this.typeName = typeName;
        this.typeNameAsString = typeNameAsString;
        this.dataType = dataType;
    }

    @CalledByNative
    public @AttributeTypeName int getTypeName() {
        return typeName;
    }
}
