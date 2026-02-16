// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.autofill_ai.AttributeInstance.DateValue;
import org.chromium.components.autofill.autofill_ai.AttributeInstance.StringValue;

import java.time.LocalDate;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/** Java representation of an Autofill AI EntityInstance. */
@JNINamespace("autofill")
@NullMarked
public class EntityInstance {
    private final String mGUID;
    private final @RecordType int mRecordType;
    private final EntityType mEntityType;
    private final Map<AttributeType, AttributeInstance> mAttributes = new HashMap<>();
    private final EntityMetadata mMetadata;

    /** Builder for the {@link EntityInstance}. */
    public static final class Builder {
        private String mGUID = "";
        private @RecordType int mRecordType = RecordType.LOCAL;
        private final EntityType mEntityType;
        private final List<AttributeInstance> mAttributes = new ArrayList<>();
        private @Nullable LocalDate mModifiedDate;
        private @Nullable Integer mUseCount;

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

        public Builder addAttribute(AttributeInstance attribute) {
            mAttributes.add(attribute);
            return this;
        }

        public Builder setModifiedDate(LocalDate modifiedDate) {
            mModifiedDate = modifiedDate;
            return this;
        }

        public Builder setUseCount(int useCount) {
            mUseCount = useCount;
            return this;
        }

        public EntityInstance build() {
            if (mModifiedDate == null) {
                throw new IllegalStateException("mModifiedDate cannot be null");
            }
            if (mUseCount == null) {
                throw new IllegalStateException("mUseCount cannot be null");
            }
            EntityMetadata metadata =
                    new EntityMetadata(
                            mModifiedDate.getDayOfMonth(),
                            mModifiedDate.getMonthValue(),
                            mModifiedDate.getYear(),
                            mUseCount);
            return new EntityInstance(mGUID, mRecordType, mEntityType, mAttributes, metadata);
        }
    }

    @CalledByNative
    private EntityInstance(
            @JniType("std::string") String guid,
            @RecordType int recordType,
            @JniType("autofill::EntityTypeAndroid") EntityType entityType,
            @JniType("std::vector<autofill::AttributeInstanceAndroid>")
                    List<AttributeInstance> attributes,
            @JniType("autofill::EntityMetadataAndroid") EntityMetadata metadata) {
        mGUID = guid;
        mRecordType = recordType;
        mEntityType = entityType;
        mMetadata = metadata;
        for (AttributeInstance attribute : attributes) {
            assert !mAttributes.containsKey(attribute.getAttributeType())
                    : "Duplicate attribute: " + attribute.getAttributeType().getTypeName();
            mAttributes.put(attribute.getAttributeType(), attribute);
        }
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
            getAttributes() {
        return new ArrayList<>(mAttributes.values());
    }

    public @Nullable AttributeInstance getAttribute(AttributeType attributeType) {
        return mAttributes.get(attributeType);
    }

    public void setAttributeValue(AttributeType attributeType, String value) {
        switch (attributeType.getDataType()) {
            case DataType.NAME:
            case DataType.STATE:
            case DataType.STRING:
            case DataType.COUNTRY:
                mAttributes.put(
                        attributeType,
                        new AttributeInstance(attributeType, new StringValue(value)));
                break;
            case DataType.DATE:
                mAttributes.put(
                        attributeType, new AttributeInstance(attributeType, new DateValue(value)));
                break;
            default:
                assert false : "Unhandled attribute data type: " + attributeType.getDataType();
        }
    }

    @CalledByNative
    public EntityMetadata getMetadata() {
        return mMetadata;
    }
}
