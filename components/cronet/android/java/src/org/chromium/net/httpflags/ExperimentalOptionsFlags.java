// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;

import java.util.Map;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Translates HTTP flags into Cronet experimental options overrides.
 *
 * <p>Experimental options are configured as JSON by the application builder. This class allows
 * overriding individual experimental options using HTTP flags matching the naming convention:
 * CronetExperimentalOption_<Section>_KEY_<Option>.
 *
 * <p>For example, CronetExperimentalOption_QUIC_KEY_idle_connection_timeout_seconds = 60 sets or
 * replaces the idle_connection_timeout_seconds option in the QUIC section.
 */
public final class ExperimentalOptionsFlags {
    private static final String TAG = "ExpOptionsFlags";

    /** HTTP flags that start with this prefix will be parsed as experimental options overrides. */
    @VisibleForTesting static final String FLAG_PREFIX = "CronetExperimentalOption_";

    private static final Pattern FLAG_NAME_PATTERN =
            Pattern.compile("^" + Pattern.quote(FLAG_PREFIX) + "(.+?)_KEY_(.+)$");

    private ExperimentalOptionsFlags() {}

    /**
     * Applies HTTP flags overrides to the provided experimental options JSON string.
     *
     * @param appOptionsJson The base experimental options JSON string from the engine builder.
     * @param flags The map of resolved HTTP flags to extract overrides from.
     * @return The effective experimental options JSON string after applying flag overrides.
     */
    @Nullable
    public static String applyOverrides(
            @Nullable String appOptionsJson, Map<String, ResolvedFlags.Value> flags) {
        JSONObject root = null;
        for (Map.Entry<String, ResolvedFlags.Value> entry : flags.entrySet()) {
            ParsedOverride override = parseOverride(entry.getKey(), entry.getValue());
            if (override == null) {
                continue;
            }
            if (root == null) {
                root = parseBaseJson(appOptionsJson);
                if (root == null) {
                    return appOptionsJson;
                }
            }
            applySingleOverride(root, override);
        }
        return root != null ? root.toString() : appOptionsJson;
    }

    @Nullable
    private static JSONObject parseBaseJson(@Nullable String appOptionsJson) {
        if (appOptionsJson == null || appOptionsJson.isEmpty()) {
            return new JSONObject();
        }
        try {
            return new JSONObject(appOptionsJson);
        } catch (JSONException e) {
            // Currently Cronet doesn't crash if the experimental options JSON fails to parse, so
            // we shouldn't crash here either.
            Log.w(TAG, "Failed to parse base experimental options JSON", e);
            return null;
        }
    }

    @VisibleForTesting
    static final class ParsedOverride {
        final String mSectionName;
        final String mOptionName;
        final Object mValue;

        ParsedOverride(String sectionName, String optionName, Object value) {
            mSectionName = Objects.requireNonNull(sectionName, "sectionName cannot be null");
            mOptionName = Objects.requireNonNull(optionName, "optionName cannot be null");
            mValue = Objects.requireNonNull(value, "value cannot be null");
        }
    }

    @Nullable
    @VisibleForTesting
    static ParsedOverride parseOverride(String flagName, ResolvedFlags.Value flagValue) {
        if (!flagName.startsWith(FLAG_PREFIX)) {
            return null;
        }

        Matcher matcher = FLAG_NAME_PATTERN.matcher(flagName);
        if (!matcher.matches()) {
            throw new IllegalArgumentException(
                    "Failed to parse experimental options flag override `" + flagName + "`");
        }

        String sectionName = matcher.group(1);
        String optionName = matcher.group(2);
        Object jsonValue = fromResolvedValue(flagName, flagValue);
        return new ParsedOverride(sectionName, optionName, jsonValue);
    }

    private static Object fromResolvedValue(String flagName, ResolvedFlags.Value value) {
        switch (value.getType()) {
            case ResolvedFlags.Value.Type.BOOL:
                return value.getBoolValue();
            case ResolvedFlags.Value.Type.INT:
                return value.getIntValue();
            case ResolvedFlags.Value.Type.FLOAT:
                return (double) value.getFloatValue();
            case ResolvedFlags.Value.Type.STRING:
                return value.getStringValue();
            case ResolvedFlags.Value.Type.BYTES:
                throw new IllegalArgumentException(
                        "Failed to parse experimental options flag override `"
                                + flagName
                                + "`: Bytes values are not supported");
            default:
                throw new IllegalArgumentException(
                        "Failed to parse experimental options flag override `"
                                + flagName
                                + "`: Unsupported value type "
                                + value.getType());
        }
    }

    private static void applySingleOverride(JSONObject root, ParsedOverride override) {
        try {
            if (!root.has(override.mSectionName)) {
                root.put(override.mSectionName, new JSONObject());
            }
            root.getJSONObject(override.mSectionName).put(override.mOptionName, override.mValue);
        } catch (JSONException e) {
            throw new IllegalArgumentException(
                    "Failed to apply experimental options flag override for section `"
                            + override.mSectionName
                            + "`, option `"
                            + override.mOptionName
                            + "`",
                    e);
        }
    }
}
