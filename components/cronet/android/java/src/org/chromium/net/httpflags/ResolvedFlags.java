// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.StringTokenizer;

/**
 * Holds the effective HTTP flags that apply to a given instance of the Cronet library.
 *
 * <p>Cronet business logic code is expected to use this class to enquire about the HTTP flag values
 * that it should use.
 */
public final class ResolvedFlags {
    /**
     * Provides type-safe access to the value of a given HTTP flag.
     *
     * <p>This object can never hold a null flag value.
     */
    public static final class Value {
        public static enum Type {
            BOOL,
            INT,
            FLOAT,
            STRING,
            BYTES
        }

        private final Object mValue;

        @Nullable
        private static Value resolve(FlagValue flagValue, String appId, int[] cronetVersion) {
            for (var constrainedValue : flagValue.getConstrainedValuesList()) {
                if ((constrainedValue.hasAppId() && !constrainedValue.getAppId().equals(appId))
                        || (constrainedValue.hasMinVersion()
                                && !matchesVersion(
                                        cronetVersion,
                                        parseVersionString(constrainedValue.getMinVersion())))) {
                    continue;
                }
                return fromConstrainedValue(constrainedValue);
            }
            return null;
        }

        private static boolean matchesVersion(int[] cronetVersion, int[] minVersion) {
            for (int i = 0; i < Math.max(cronetVersion.length, minVersion.length); i++) {
                int cronetComponent = i < cronetVersion.length ? cronetVersion[i] : 0;
                int minComponent = i < minVersion.length ? minVersion[i] : 0;
                if (cronetComponent > minComponent) {
                    return true;
                } else if (cronetComponent < minComponent) {
                    return false;
                }
            }
            return true;
        }

        private static Value fromConstrainedValue(FlagValue.ConstrainedValue constrainedValue) {
            FlagValue.ConstrainedValue.ValueCase valueCase = constrainedValue.getValueCase();
            switch (valueCase) {
                case BOOL_VALUE:
                    return new Value(constrainedValue.getBoolValue());
                case INT_VALUE:
                    return new Value(constrainedValue.getIntValue());
                case FLOAT_VALUE:
                    return new Value(constrainedValue.getFloatValue());
                case STRING_VALUE:
                    return new Value(constrainedValue.getStringValue());
                case BYTES_VALUE:
                    return new Value(constrainedValue.getBytesValue());
                case VALUE_NOT_SET:
                    return null;
                default:
                    throw new IllegalArgumentException(
                            "Flag value uses unknown value type " + valueCase);
            }
        }

        @VisibleForTesting
        public Value(boolean value) {
            mValue = value;
        }

        @VisibleForTesting
        public Value(long value) {
            mValue = value;
        }

        @VisibleForTesting
        public Value(float value) {
            mValue = value;
        }

        @VisibleForTesting
        public Value(String value) {
            mValue = value;
        }

        @VisibleForTesting
        public Value(ByteString value) {
            mValue = value;
        }

        public Type getType() {
            if (mValue instanceof Boolean) {
                return Type.BOOL;
            } else if (mValue instanceof Long) {
                return Type.INT;
            } else if (mValue instanceof Float) {
                return Type.FLOAT;
            } else if (mValue instanceof String) {
                return Type.STRING;
            } else if (mValue instanceof ByteString) {
                return Type.BYTES;
            } else {
                throw new IllegalStateException(
                        "Unexpected flag value type: " + mValue.getClass().getName());
            }
        }

        private void checkType(Type requestedType) {
            Type actualType = getType();
            if (requestedType != actualType) {
                throw new IllegalStateException(
                        "Attempted to access flag value as "
                                + requestedType
                                + ", but actual type is "
                                + actualType);
            }
        }

        /** @throws IllegalStateException Iff {@link #getType} is not {@link Type#BOOL} */
        public boolean getBoolValue() {
            checkType(Type.BOOL);
            return (Boolean) mValue;
        }

        /** @throws IllegalStateException Iff {@link #getType} is not {@link Type#INT} */
        public long getIntValue() {
            checkType(Type.INT);
            return (Long) mValue;
        }

        /** @throws IllegalStateException Iff {@link #getType} is not {@link Type#FLOAT} */
        public float getFloatValue() {
            checkType(Type.FLOAT);
            return (Float) mValue;
        }

        /** @throws IllegalStateException Iff {@link #getType} is not {@link Type#STRING} */
        public String getStringValue() {
            checkType(Type.STRING);
            return (String) mValue;
        }

        /** @throws IllegalStateException Iff {@link #getType} is not {@link Type#BYTES} */
        public ByteString getBytesValue() {
            checkType(Type.BYTES);
            return (ByteString) mValue;
        }
    }

    private final Map<String, Value> mFlags;

    /**
     * Computes effective flag values based on the contents of a {@link Flags} proto.
     *
     * <p>This method will resolve {@link FlagValue.ConstrainedValue} filters according to the
     * other arguments, producing the final values that should apply to the caller.
     *
     * <p>Note that a {@link FlagValue} that has no {@link FlagValue.ConstrainedValue} entry, or
     * where the matching entry has no value set, will not be mentioned at all in the resulting
     * {@link #flags}.
     *
     * @param flags The {@link Flags} proto to extract the flag values from. This would normally be
     *              the return value of {@link HttpFlagsLoader#load}.
     * @param appId The App ID for resolving the {@link FlagValue.ConstrainedValue#getAppId} field.
     *              This would normally be the return value of
     *              {@link android.content.Context#getPackageName}.
     * @param cronetVersion The version to use for filtering against the {@link
     *                      FlagValue.ConstrainedValue#getMinVersion} field.
     */
    public static ResolvedFlags resolve(Flags flags, String appId, String cronetVersion) {
        int[] parsedCronetVersion = parseVersionString(cronetVersion);
        Map<String, Value> resolvedFlags = new HashMap<String, Value>();
        for (var flag : flags.getFlagsMap().entrySet()) {
            try {
                Value value = Value.resolve(flag.getValue(), appId, parsedCronetVersion);
                if (value == null) continue;
                resolvedFlags.put(flag.getKey(), value);
            } catch (RuntimeException exception) {
                throw new IllegalArgumentException(
                        "Unable to resolve HTTP flag `" + flag.getKey() + "`", exception);
            }
        }
        return new ResolvedFlags(resolvedFlags);
    }

    @VisibleForTesting
    public ResolvedFlags(Map<String, Value> flags) {
        mFlags = flags;
    }

    /**
     * @return The effective HTTP flag values, keyed by flag name. Neither keys nor values can be
     * null. Only flags that have actual values are included in the result.
     */
    public Map<String, Value> flags() {
        return Collections.unmodifiableMap(mFlags);
    }

    private static int[] parseVersionString(String versionString) {
        try {
            if (versionString.isEmpty()) {
                throw new IllegalArgumentException("Version string is empty");
            }
            StringTokenizer tokenizer = new StringTokenizer(versionString, ".");
            int[] components = new int[tokenizer.countTokens()];
            for (int i = 0; i < components.length; i++) {
                components[i] = Integer.parseInt(tokenizer.nextToken());
            }
            return components;
        } catch (RuntimeException exception) {
            throw new IllegalArgumentException(
                    "Unable to parse HTTP flags version string: `" + versionString + "`",
                    exception);
        }
    }
}
