// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Java representation of an Autofill AI EntityInstance. */
@JNINamespace("autofill")
@NullMarked
public class EntityInstance {
    private final String mGUID;
    private final @RecordType int mRecordType;
    private final EntityType mEntityType;
    private final List<AttributeInstance> mAttributeValues;

    /** Builder for the {@link EntityInstance}. */
    public static final class Builder {
        private String mGUID = "";
        private @RecordType int mRecordType = RecordType.LOCAL;
        private final EntityType mEntityType;
        private final List<AttributeInstance> mAttributeValues = new ArrayList<>();

        public Builder(EntityType entityType) {
            mEntityType = Objects.requireNonNull(entityType, "Entity type cannot be null");
        }

        public Builder setGUID(String guid) {
            mGUID = guid;
            return this;
        }

        public Builder setRecordType(@RecordType int recordType) {
            mRecordType = recordType;
            return this;
        }

        public Builder addAttributeValue(AttributeInstance attributeValue) {
            mAttributeValues.add(attributeValue);
            return this;
        }

        public EntityInstance build() {
            return new EntityInstance(mGUID, mRecordType, mEntityType, mAttributeValues);
        }
    }

    @CalledByNative
    private EntityInstance(
            @JniType("std::string") String guid,
            @RecordType int recordType,
            @JniType("autofill::EntityTypeAndroid") EntityType entityType,
            @JniType("std::vector<autofill::AttributeInstanceAndroid>")
                    List<AttributeInstance> attributeValues) {
        mGUID = guid;
        mRecordType = recordType;
        mEntityType = entityType;
        mAttributeValues = attributeValues;
    }

    @CalledByNative
    public @JniType("std::string") String getGUID() {
        return mGUID;
    }

    @CalledByNative
    public @JniType("autofill::EntityInstance::RecordType") @RecordType int getRecordType() {
        return mRecordType;
    }

    @CalledByNative
    public @JniType("autofill::EntityTypeAndroid") EntityType getEntityType() {
        return mEntityType;
    }

    @CalledByNative
    public @JniType("std::vector<autofill::AttributeInstanceAndroid>") List<AttributeInstance>
            getAttributeValues() {
        return mAttributeValues;
    }
}
