// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.autofill_ai.AttributeInstance.DateValue;
import org.chromium.components.autofill.autofill_ai.AttributeInstance.StringValue;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/** Java representation of an Autofill AI EntityInstance. */
@JNINamespace("autofill")
@NullMarked
public class EntityInstance {
    private final @RecordType int mRecordType;
    private final EntityType mEntityType;
    private final Map<AttributeType, AttributeInstance> mAttributes = new HashMap<>();
    private final String mNickname;
    private final EntityMetadata mMetadata;
    private final boolean mRequiresReauthToSee;
    private final boolean mIsMaskedServerEntity;

    /** Builder for the {@link EntityInstance}. */
    public static final class Builder {
        private String mGuid = "";
        private @RecordType int mRecordType = RecordType.LOCAL;
        private final EntityType mEntityType;
        private final List<AttributeInstance> mAttributes = new ArrayList<>();
        private String mNickname = "";
        private long mModifiedDateMillis;
        private @Nullable Integer mUseCount;
        private boolean mRequiresReauthToSee;
        private boolean mIsMaskedServerEntity;

        public Builder(EntityType entityType) {
            mEntityType = Objects.requireNonNull(entityType, "Entity type cannot be null");
        }

        public Builder setGuid(String guid) {
            mGuid = guid;
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

        public Builder setNickname(String nickname) {
            mNickname = nickname;
            return this;
        }

        public Builder setModifiedDate(long modifiedDateMillis) {
            mModifiedDateMillis = modifiedDateMillis;
            return this;
        }

        public Builder setUseCount(int useCount) {
            mUseCount = useCount;
            return this;
        }

        public Builder setRequiresReauthToSee(boolean requiresReauthToSee) {
            mRequiresReauthToSee = requiresReauthToSee;
            return this;
        }

        public Builder setIsMaskedServerEntity(boolean isMaskedServerEntity) {
            mIsMaskedServerEntity = isMaskedServerEntity;
            return this;
        }

        public EntityInstance build() {
            if (mModifiedDateMillis == 0) {
                mModifiedDateMillis = TimeUtils.currentTimeMillis();
            }
            if (mUseCount == null) {
                throw new IllegalStateException("mUseCount cannot be null");
            }
            EntityMetadata metadata = new EntityMetadata(mGuid, mModifiedDateMillis, mUseCount);
            return new EntityInstance(
                    mRecordType,
                    mEntityType,
                    mAttributes,
                    mNickname,
                    metadata,
                    mRequiresReauthToSee,
                    mIsMaskedServerEntity);
        }
    }

    @CalledByNative
    private EntityInstance(
            @RecordType int recordType,
            @JniType("autofill::EntityTypeAndroid") EntityType entityType,
            @JniType("std::vector<autofill::AttributeInstanceAndroid>")
                    List<AttributeInstance> attributes,
            @JniType("std::string") String nickname,
            @JniType("autofill::EntityMetadataAndroid") EntityMetadata metadata,
            boolean requiresReauthToSee,
            boolean isMaskedServerEntity) {
        mRecordType = recordType;
        mEntityType = entityType;
        mMetadata = metadata;
        for (AttributeInstance attribute : attributes) {
            assert !mAttributes.containsKey(attribute.getAttributeType())
                    : "Duplicate attribute: " + attribute.getAttributeType().getTypeName();
            mAttributes.put(attribute.getAttributeType(), attribute);
        }
        mNickname = nickname;
        mRequiresReauthToSee = requiresReauthToSee;
        mIsMaskedServerEntity = isMaskedServerEntity;
    }

    @CalledByNative
    public @JniType("std::string") String getGuid() {
        return mMetadata.getGuid();
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

    @CalledByNative
    public @JniType("std::string") String getNickname() {
        return mNickname;
    }

    public @Nullable AttributeInstance getAttribute(AttributeType attributeType) {
        return mAttributes.get(attributeType);
    }

    public boolean hasAttribute(AttributeType attributeType) {
        return mAttributes.containsKey(attributeType);
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

    @CalledByNative
    public boolean requiresReauthToSee() {
        return mRequiresReauthToSee;
    }

    @CalledByNative
    public boolean isMaskedServerEntity() {
        return mIsMaskedServerEntity;
    }
}
