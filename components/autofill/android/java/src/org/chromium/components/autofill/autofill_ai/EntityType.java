// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents information of an Autofill AI entity type, used in the management page to build the
 * UI.
 */
@JNINamespace("autofill")
@NullMarked
public class EntityType {
    // This maps to a C++ enum which defines the name/type of the entity.
    public final @EntityTypeName int typeName;
    // When `isReadOnly` is true, this entity type does not allow adding, deleting or editing.
    public final boolean isReadOnly;
    // Used to sort entity types and groups and as title of each entity group in the list of
    // entities.
    public final String typeNameAsString;
    // Used as title in the add entity dialog.
    public final String addEntityTypeString;
    // Used as title in the edit entity dialog.
    public final String editEntityTypeString;
    // Used as title in the delete entity dialog.
    public final String deleteEntityTypeString;

    @CalledByNative
    public EntityType(
            @EntityTypeName int typeName,
            boolean isReadOnly,
            @JniType("std::u16string") String typeNameAsString,
            @JniType("std::u16string") String addEntityTypeString,
            @JniType("std::u16string") String editEntityTypeString,
            @JniType("std::u16string") String deleteEntityTypeString) {
        this.typeName = typeName;
        this.isReadOnly = isReadOnly;
        this.typeNameAsString = typeNameAsString;
        this.addEntityTypeString = addEntityTypeString;
        this.editEntityTypeString = editEntityTypeString;
        this.deleteEntityTypeString = deleteEntityTypeString;
    }

    @CalledByNative
    public @EntityTypeName int getTypeName() {
        return typeName;
    }
}
