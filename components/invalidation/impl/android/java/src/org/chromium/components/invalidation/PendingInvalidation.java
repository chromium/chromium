// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import android.os.Bundle;
import android.util.Base64;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.invalidation.SerializedInvalidation.Invalidation;

import java.io.IOException;
import java.util.Locale;

/**
 * A container class that stores the received invalidations.
 * It plays the role of abstracting conversions to and from other storage types like String
 * (storing in prefStore) and Bundle (ContentProvider).
 */
public class PendingInvalidation {
    private static final String TAG = "invalidation";

    private static final String INVALIDATION_OBJECT_SOURCE_KEY = "objectSource";
    private static final String INVALIDATION_OBJECT_ID_KEY = "objectId";
    private static final String INVALIDATION_VERSION_KEY = "version";
    private static final String INVALIDATION_PAYLOAD_KEY = "payload";

    public final String mObjectId;
    public final int mObjectSource;
    public final long mVersion;
    public final String mPayload;

    /**
     * A constructor used in tests only.
     */
    @VisibleForTesting
    public PendingInvalidation(String id, int source, long version, String payload) {
        mObjectId = id;
        mObjectSource = source;
        mVersion = version;
        mPayload = payload;
    }

    /**
     * Parse a PendingInvalidation from a bundle.
     */
    public PendingInvalidation(Bundle bundle) {
        // bundle.get* methods helpfully give compatible fallback values so we don't handle that.
        mObjectId = bundle.getString(INVALIDATION_OBJECT_ID_KEY);
        mObjectSource = bundle.getInt(INVALIDATION_OBJECT_SOURCE_KEY);
        mVersion = bundle.getLong(INVALIDATION_VERSION_KEY);
        mPayload = bundle.getString(INVALIDATION_PAYLOAD_KEY);
    }

    /**
     * Encode an invalidation into a bundle.
     */
    public static Bundle createBundle(String id, int source, long version, String payload) {
        Bundle bundle = new Bundle();
        bundle.putString(INVALIDATION_OBJECT_ID_KEY, id);
        bundle.putInt(INVALIDATION_OBJECT_SOURCE_KEY, source);
        bundle.putLong(INVALIDATION_VERSION_KEY, version);
        bundle.putString(INVALIDATION_PAYLOAD_KEY, payload);
        return bundle;
    }

    /**
     * Encode an invalidation into a String.
     * The invalidation is first encoded as {@link SerializedInvalidation.Invalidation}, which is
     * further encoded into a String of Base64 encoding.
     * Do not call for mObjectSource == 0, which is an invalidation for all types and is handled
     * specially.
     */
    public String encodeToString() {
        assert mObjectSource != 0;
        Invalidation.Builder invalidationBuilder =
                Invalidation.newBuilder().setObjectSource(mObjectSource);
        // The following setters update internal state of |invalidationBuilder|.
        if (mObjectId != null) invalidationBuilder.setObjectId(mObjectId);
        if (mVersion != 0L) invalidationBuilder.setVersion(mVersion);
        if (mPayload != null) invalidationBuilder.setPayload(mPayload);
        return Base64.encodeToString(invalidationBuilder.build().toByteArray(), Base64.DEFAULT);
    }

    /**
     * Decode the invalidation encoded as a String into a Bundle.
     * Return value is {@code null} if the string could not be parsed or is an invalidation for all.
     */
    @Nullable
    public static Bundle decodeToBundle(String encoded) {
        Invalidation invalidation = decodeToInvalidation(encoded);
        if (invalidation == null) return null;
        return createBundle(invalidation.hasObjectId() ? invalidation.getObjectId() : null,
                invalidation.getObjectSource(),
                invalidation.hasVersion() ? invalidation.getVersion() : 0L,
                invalidation.hasPayload() ? invalidation.getPayload() : null);
    }

    /**
     * Decode the invalidation encoded as a String into a PendingInvalidation.
     * Return value is {@code null} if the string could not be parsed or is an invalidation for all.
     */
    @Nullable
    public static PendingInvalidation decodeToPendingInvalidation(String encoded) {
        Invalidation invalidation = decodeToInvalidation(encoded);
        if (invalidation == null) return null;
        return new PendingInvalidation(
                invalidation.hasObjectId() ? invalidation.getObjectId() : null,
                invalidation.getObjectSource(), invalidation.getVersion(),
                invalidation.hasPayload() ? invalidation.getPayload() : null);
    }

    @Nullable
    private static Invalidation decodeToInvalidation(String encoded) {
        assert encoded != null;
        byte[] decoded = Base64.decode(encoded, Base64.DEFAULT);
        Invalidation invalidation;
        try {
            invalidation = Invalidation.parseFrom(decoded);
        } catch (IOException e) {
            Log.e(TAG, "Could not parse the serialized invalidations.", e);
            return null;
        }
        assert invalidation != null;
        if (!invalidation.hasObjectSource() || invalidation.getObjectSource() == 0) return null;
        return invalidation;
    }

    public String toDebugString() {
        String payload = mPayload == null ? "null" : String.valueOf(mPayload.length());
        return String.format(Locale.US, "objectSrc:%d,objectId:%s,version:%d,payload(length):%s",
                mObjectSource, mObjectId, mVersion, payload);
    }

    @VisibleForTesting
    @Override
    public boolean equals(Object other) {
        if (other == null) return false;
        if (other == this) return true;
        if (!(other instanceof PendingInvalidation)) return false;
        PendingInvalidation otherInvalidation = (PendingInvalidation) other;
        if (mObjectSource != otherInvalidation.mObjectSource) return false;
        if (mObjectId == null) {
            if (otherInvalidation.mObjectId != null) return false;
        } else {
            if (!mObjectId.equals(otherInvalidation.mObjectId)) return false;
        }
        if (mVersion != otherInvalidation.mVersion) return false;
        if (mPayload == null) {
            if (otherInvalidation.mPayload != null) return false;
        } else {
            if (!mPayload.equals(otherInvalidation.mPayload)) return false;
        }
        return true;
    }

    @Override
    public int hashCode() {
        int hashCode = 0;
        if (mObjectId != null) hashCode ^= mObjectId.hashCode();
        hashCode ^= mObjectSource;
        hashCode ^= Long.valueOf(mVersion).hashCode();
        if (mPayload != null) hashCode ^= mPayload.hashCode();
        return hashCode;
    }

    @Override
    public String toString() {
        return String.format(Locale.US, "objectSrc:%d,objectId:%s,version:%d,payload:%s",
                mObjectSource, mObjectId, mVersion, mPayload);
    }
}
