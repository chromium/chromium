// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/** Utility class for bridging the gap between HTTP flags and the native `base::Feature` framework. */
public final class BaseFeature {
    /** HTTP flags that start with this name will be turned into base::Feature overrides. */
    @VisibleForTesting public static final String FLAG_PREFIX = "ChromiumBaseFeature_";

    /**
     * If this delimiter is found in an HTTP flag name, the HTTP flag is assumed to refer to a
     * base::Feature param. The part before the delimiter is the base::Feature name, and the part
     * after the delimiter is the param name.
     */
    @VisibleForTesting public static final String PARAM_DELIMITER = "_PARAM_";

    private BaseFeature() {}

    /**
     * Turns a set of resolved HTTP flags into native {@code base::Feature} overrides.
     *
     * <p>Only HTTP flags whose name start with {@link #FLAG_PREFIX} are considered.
     *
     * <p>If the flag name does not include {@link #PARAM_DELIMITER}, then the flag is treated as
     * a state override for a base::Feature named after the HTTP flag (without the
     * {@link #FLAG_PREFIX} prefix). In that case the flag value is required to be a boolean. The
     * state is overridden to the "enabled" state if the flag value is true, or to the "disabled"
     * state if the flag value is false.
     *
     * <p>If the flag name does include {@link #PARAM_DELIMITER}, then the flag is treated as a
     * base::Feature param override. In that case the part after {@link #FLAG_PREFIX} but before
     * {@link #PARAM_DELIMITER} is the name of the base::Feature, and the part after {@link
     * #PARAM_DELIMITER} is the name of the param. The param value is the flag value, converted to
     * string in such a way as to allow base::FeatureParam code to unparse it.
     *
     * <p>Examples:
     * <ul>
     * <li>An HTTP flag named {@code ChromiumBaseFeature_LogMe} with value {@code true} enables the
     * {@code LogMe} base::Feature.
     * <li>An HTTP flag named {@code ChromiumBaseFeature_LogMe_PARAM_marker} with value {@code
     * "foobar"} sets the {@code marker} param on the {@code LogMe} base::Feature to {@code
     * "foobar"}.
     * </ul>
     *
     * @throws IllegalArgumentException if the flags are invalid or otherwise can't be parsed
     *
     * @see org.chromium.net.impl.CronetLibraryLoader#getBaseFeatureOverrides
     */
    public static BaseFeatureOverrides getOverrides(ResolvedFlags flags) {
        Map<String, BaseFeatureOverrides.FeatureState.Builder> featureStateBuilders =
                new HashMap<String, BaseFeatureOverrides.FeatureState.Builder>();

        for (Map.Entry<String, ResolvedFlags.Value> flag : flags.flags().entrySet()) {
            try {
                applyOverride(flag.getKey(), flag.getValue(), featureStateBuilders);
            } catch (RuntimeException exception) {
                throw new IllegalArgumentException(
                        "Could not parse HTTP flag `"
                                + flag.getKey()
                                + "` as a base::Feature override",
                        exception);
            }
        }

        BaseFeatureOverrides.Builder builder = BaseFeatureOverrides.newBuilder();
        for (Map.Entry<String, BaseFeatureOverrides.FeatureState.Builder> featureStateBuilder :
                featureStateBuilders.entrySet()) {
            builder.putFeatureStates(
                    featureStateBuilder.getKey(), featureStateBuilder.getValue().build());
        }
        return builder.build();
    }

    private static void applyOverride(
            String flagName,
            ResolvedFlags.Value flagValue,
            Map<String, BaseFeatureOverrides.FeatureState.Builder> featureStateBuilders) {
        ParsedFlagName parsedFlagName = parseFlagName(flagName);
        if (parsedFlagName == null) return;

        BaseFeatureOverrides.FeatureState.Builder featureStateBuilder =
                featureStateBuilders.get(parsedFlagName.featureName);
        if (featureStateBuilder == null) {
            featureStateBuilder = BaseFeatureOverrides.FeatureState.newBuilder();
            featureStateBuilders.put(parsedFlagName.featureName, featureStateBuilder);
        }

        if (parsedFlagName.paramName == null) {
            applyStateOverride(flagValue, featureStateBuilder);
        } else {
            applyParamOverride(parsedFlagName.paramName, flagValue, featureStateBuilder);
        }
    }

    private static final class ParsedFlagName {
        public String featureName;
        @Nullable public String paramName;
    }

    @Nullable
    private static ParsedFlagName parseFlagName(String flagName) {
        if (!flagName.startsWith(FLAG_PREFIX)) return null;
        String flagNameWithoutPrefix = flagName.substring(FLAG_PREFIX.length());

        ParsedFlagName parsed = new ParsedFlagName();

        int delimiterIndex = flagNameWithoutPrefix.indexOf(PARAM_DELIMITER);
        if (delimiterIndex < 0) {
            parsed.featureName = flagNameWithoutPrefix;
        } else {
            parsed.featureName = flagNameWithoutPrefix.substring(0, delimiterIndex);
            parsed.paramName =
                    flagNameWithoutPrefix.substring(delimiterIndex + PARAM_DELIMITER.length());
        }
        return parsed;
    }

    private static void applyStateOverride(
            ResolvedFlags.Value value,
            BaseFeatureOverrides.FeatureState.Builder featureStateBuilder) {
        ResolvedFlags.Value.Type valueType = value.getType();
        if (valueType != ResolvedFlags.Value.Type.BOOL) {
            throw new IllegalArgumentException(
                    "HTTP flag has type "
                            + valueType
                            + ", but only boolean flags are supported as base::Feature overrides");
        }
        featureStateBuilder.setEnabled(value.getBoolValue());
    }

    private static void applyParamOverride(
            String paramName,
            ResolvedFlags.Value value,
            BaseFeatureOverrides.FeatureState.Builder featureStateBuilder) {
        ResolvedFlags.Value.Type valueType = value.getType();
        ByteString rawValue;
        switch (valueType) {
            case BOOL:
                rawValue =
                        ByteString.copyFrom(
                                value.getBoolValue() ? "true" : "false", StandardCharsets.UTF_8);
                break;
            case INT:
                rawValue =
                        ByteString.copyFrom(
                                Long.toString(value.getIntValue(), /* radix= */ 10),
                                StandardCharsets.UTF_8);
                break;
            case FLOAT:
                // TODO: if the value is "weird" (e.g. NaN, infinities) this probably won't produce
                // something that the Chromium feature param code can parse. As a workaround, the
                // user can use a string-valued flag to directly feed the value to be parsed.
                rawValue =
                        ByteString.copyFrom(
                                Float.toString(value.getFloatValue()), StandardCharsets.UTF_8);
                break;
            case STRING:
                rawValue = ByteString.copyFrom(value.getStringValue(), StandardCharsets.UTF_8);
                break;
            case BYTES:
                rawValue = value.getBytesValue();
                break;
            default:
                throw new UnsupportedOperationException(
                        "Unsupported HTTP flag value type for base::Feature param `"
                                + paramName
                                + "`: "
                                + valueType);
        }
        featureStateBuilder.putParams(paramName, rawValue);
    }
}
