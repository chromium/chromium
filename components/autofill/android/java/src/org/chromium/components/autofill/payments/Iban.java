// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.IbanRecordType;

import java.util.Objects;

/** Autofill IBAN information. */
@JNINamespace("autofill")
@NullMarked
public class Iban {
    private final @Nullable String mGuid;
    private final @Nullable Long mInstrumentId;

    // Obfuscated IBAN value. This is used for displaying the IBAN in the Payment methods page.
    private final String mLabel;

    private String mNickname;
    private final @IbanRecordType int mRecordType;
    // Value is empty for server IBAN.
    private @Nullable String mValue;

    private Iban(
            @Nullable String guid,
            @Nullable Long instrumentId,
            String label,
            String nickname,
            @IbanRecordType int recordType,
            @Nullable String value) {
        mGuid = guid;
        mInstrumentId = instrumentId;
        mLabel = Objects.requireNonNull(label, "Label can't be null");
        mNickname = Objects.requireNonNull(nickname, "Nickname can't be null");
        mRecordType = recordType;
        mValue = value;
    }

    // Creates an Iban instance that is not stored on a server nor locally,
    // yet. This Iban has type IbanRecordType.UNKNOWN and has neither a
    // Guid nor an instrumentId.
    @CalledByNative
    public static Iban createEphemeral(
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String nickname,
            @JniType("std::u16string") String value) {
        return new Iban.Builder()
                .setLabel(label)
                .setNickname(nickname)
                .setRecordType(IbanRecordType.UNKNOWN)
                .setValue(value)
                .build();
    }

    @CalledByNative
    public static Iban createLocal(
            @JniType("std::string") String guid,
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String nickname,
            @JniType("std::u16string") String value) {
        return new Iban.Builder()
                .setGuid(guid)
                .setLabel(label)
                .setNickname(nickname)
                .setRecordType(IbanRecordType.LOCAL_IBAN)
                .setValue(value)
                .build();
    }

    @CalledByNative
    public static Iban createServer(
            long instrumentId,
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String nickname,
            @JniType("std::u16string") String value) {
        return new Iban.Builder()
                .setInstrumentId(instrumentId)
                .setLabel(label)
                .setNickname(nickname)
                .setRecordType(IbanRecordType.SERVER_IBAN)
                .setValue(value)
                .build();
    }

    @CalledByNative
    public @JniType("std::string") @Nullable String getGuid() {
        assert mRecordType != IbanRecordType.SERVER_IBAN;
        return mGuid;
    }

    @CalledByNative
    public long getInstrumentId() {
        assert mInstrumentId != null;
        assert mRecordType == IbanRecordType.SERVER_IBAN;
        return mInstrumentId;
    }

    public String getLabel() {
        return mLabel;
    }

    @CalledByNative
    public @JniType("std::u16string") String getNickname() {
        return mNickname;
    }

    @CalledByNative
    public @IbanRecordType int getRecordType() {
        return mRecordType;
    }

    @CalledByNative
    public @JniType("std::u16string") @Nullable String getValue() {
        return mValue;
    }

    public void updateNickname(String nickname) {
        mNickname = nickname;
    }

    public void updateValue(String value) {
        mValue = value;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof Iban)) {
            return false;
        }

        Iban otherIban = (Iban) obj;

        return Objects.equals(mLabel, otherIban.mLabel)
                && Objects.equals(mNickname, otherIban.mNickname)
                && mRecordType == otherIban.mRecordType
                && (mRecordType != IbanRecordType.SERVER_IBAN
                        || Objects.equals(mInstrumentId, otherIban.mInstrumentId))
                && (mRecordType != IbanRecordType.LOCAL_IBAN
                        || Objects.equals(mGuid, otherIban.mGuid))
                && Objects.equals(mValue, otherIban.mValue);
    }

    @Override
    public int hashCode() {
        Object identifier = null;
        if (mRecordType == IbanRecordType.SERVER_IBAN) {
            identifier = mInstrumentId;
        } else if (mRecordType == IbanRecordType.LOCAL_IBAN) {
            identifier = mGuid;
        }
        return Objects.hash(identifier, mLabel, mNickname, mRecordType, mValue);
    }

    /** Builder for {@link Iban}. */
    public static final class Builder {
        private @Nullable String mGuid;
        private @Nullable Long mInstrumentId;
        private @Nullable String mLabel;
        private @Nullable String mNickname;
        private @IbanRecordType int mRecordType;
        private @Nullable String mValue;

        public Builder setGuid(String guid) {
            mGuid = guid;
            return this;
        }

        public Builder setInstrumentId(long instrumentId) {
            mInstrumentId = instrumentId;
            return this;
        }

        public Builder setLabel(String label) {
            mLabel = label;
            return this;
        }

        public Builder setNickname(String nickname) {
            mNickname = nickname;
            return this;
        }

        public Builder setRecordType(@IbanRecordType int recordType) {
            mRecordType = recordType;
            return this;
        }

        public Builder setValue(String value) {
            mValue = value;
            return this;
        }

        public Iban build() {
            switch (mRecordType) {
                case IbanRecordType.UNKNOWN:
                    assert mGuid == null && mInstrumentId == null
                            : "IBANs with 'UNKNOWN' record type must have an empty GUID and"
                                    + " InstrumentId.";
                    break;
                case IbanRecordType.LOCAL_IBAN:
                    assert !TextUtils.isEmpty(mGuid) && mInstrumentId == null
                            : "Local IBANs must have a non-empty GUID and null InstrumentID.";
                    break;
                case IbanRecordType.SERVER_IBAN:
                    assert mInstrumentId != null
                                    && mInstrumentId != 0L
                                    && TextUtils.isEmpty(mGuid)
                                    && TextUtils.isEmpty(mValue)
                            : "Server IBANs must have a non-zero instrumentId, empty GUID and"
                                    + " empty value.";
                    break;
                default:
                    assert false : "Unexpected record type: " + mRecordType;
                    break;
            }
            // Non-null enforcement happens inside the constructor if applicable, assume
            // non-null for all fields.
            return new Iban(
                    assumeNonNull(mGuid),
                    assumeNonNull(mInstrumentId),
                    assumeNonNull(mLabel),
                    assumeNonNull(mNickname),
                    mRecordType,
                    assumeNonNull(mValue));
        }
    }
}
