// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/**
 * Represents information of an Autofill AI entity type, used in the management page to build the
 * UI.
 */
@JNINamespace("autofill")
@NullMarked
public class EntityType {
    // This maps to a C++ enum which defines the name/type of the entity.
    private final @EntityTypeName int mTypeName;
    // When `isReadOnly` is true, this entity type does not allow adding, deleting or editing.
    private final boolean mIsReadOnly;
    // Used to sort entity types and groups and as title of each entity group in the list of
    // entities.
    private final String mTypeNameAsString;
    // Used for histogram recording.
    private final String mTypeNameAsMetricsString;
    // Used as title in the add entity dialog.
    private final String mAddEntityTypeString;
    // Used as title in the edit entity dialog.
    private final String mEditEntityTypeString;
    // Used as title in the delete entity dialog.
    private final String mDeleteEntityTypeString;
    // The complete list of attribute types this entity type supports.
    private final List<AttributeType> mAttributeTypes;

    @CalledByNative
    public EntityType(
            @EntityTypeName int typeName,
            boolean isReadOnly,
            @JniType("std::u16string") String typeNameAsString,
            @JniType("std::string") String typeNameAsMetricsString,
            @JniType("std::string") String addEntityTypeString,
            @JniType("std::string") String editEntityTypeString,
            @JniType("std::string") String deleteEntityTypeString,
            @JniType("std::vector<autofill::AttributeTypeAndroid>")
                    List<AttributeType> attributeTypes) {
        mTypeName = typeName;
        mIsReadOnly = isReadOnly;
        mTypeNameAsString = typeNameAsString;
        mTypeNameAsMetricsString = typeNameAsMetricsString;
        mAddEntityTypeString = addEntityTypeString;
        mEditEntityTypeString = editEntityTypeString;
        mDeleteEntityTypeString = deleteEntityTypeString;
        mAttributeTypes = attributeTypes;
    }

    @CalledByNative
    public @EntityTypeName int getTypeName() {
        return mTypeName;
    }

    public boolean isReadOnly() {
        return mIsReadOnly;
    }

    public String getTypeNameAsString() {
        return mTypeNameAsString;
    }

    public String getTypeNameAsMetricsString() {
        return mTypeNameAsMetricsString;
    }

    public String getAddEntityTypeString() {
        return mAddEntityTypeString;
    }

    public String getEditEntityTypeString() {
        return mEditEntityTypeString;
    }

    public String getDeleteEntityTypeString() {
        return mDeleteEntityTypeString;
    }

    public List<AttributeType> getAttributeTypes() {
        return mAttributeTypes;
    }
}
