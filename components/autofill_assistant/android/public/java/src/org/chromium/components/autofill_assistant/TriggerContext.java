// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

/**
 * Describes the context of the trigger, which contains the script parameters along with some other
 * optional parameters.
 */
public class TriggerContext {
    /**
     * Builder class for {@link TriggerContext}.
     */
    public static class Builder {
        private TriggerContext mTriggerContext;

        private Builder(TriggerContext arguments) {
            mTriggerContext = arguments;
        }

        public Builder fromBundle(@Nullable Bundle bundle) {
            if (bundle != null) {
                for (String key : bundle.keySet()) {
                    Object value = bundle.get(key);
                    if (value == null) {
                        continue;
                    }
                    if (key.startsWith(INTENT_EXTRA_DEVICE_ONLY_PREFIX)) {
                        mTriggerContext.mDeviceOnlyScriptParameters.put(
                                key.substring(INTENT_EXTRA_DEVICE_ONLY_PREFIX.length()), value);
                    } else if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                        if (key.equals(INTENT_EXTRA_PREFIX + PARAMETER_EXPERIMENT_IDS)) {
                            mTriggerContext.addExperimentIds(decode(value.toString()));
                        }
                        mTriggerContext.mScriptParameters.put(
                                key.substring(INTENT_EXTRA_PREFIX.length()), value);
                    }
                }
            }

            return this;
        }

        public Builder withInitialUrl(String url) {
            mTriggerContext.mInitialUrl = url;
            return this;
        }

        public Builder addParameter(String key, Object value) {
            mTriggerContext.mScriptParameters.put(key, value);
            return this;
        }

        public TriggerContext build() {
            return mTriggerContext;
        }
    }

    private static String decode(String value) {
        try {
            return URLDecoder.decode(value, UTF8);
        } catch (java.io.UnsupportedEncodingException e) {
            throw new IllegalStateException("Encoding not available.", e);
        }
    }

    /** Used for logging. */
    private static final String TAG = "AutofillAssistant";

    private static final String UTF8 = "UTF-8";

    /**
     * Prefix for Intent extras relevant to this feature.
     *
     * <p>Extras starting with this prefix are reported to the controller as script parameters.
     */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /**
     * Prefix for Intent extras that should remain on the device and not be sent to the backend.
     */
    private static final String INTENT_EXTRA_DEVICE_ONLY_PREFIX =
            INTENT_EXTRA_PREFIX + "device_only.";

    /** Special parameter that enables the feature. */
    public static final String PARAMETER_ENABLED = "ENABLED";

    /** Special parameter that contains the intent of the flow. */
    public static final String PARAMETER_INTENT = "INTENT";

    /**
     * Special bool parameter that MUST be present in all intents. It allows the caller to either
     * request immediate start of autobot (if set to true), or a delayed start using trigger scripts
     * (if set to false). If this is set to false, one of the trigger script parameters must be set
     * as well (@code{PARAMETER_REQUEST_TRIGGER_SCRIPT} or @code{PARAMETER_TRIGGER_SCRIPTS_BASE64}).
     */
    public static final String PARAMETER_START_IMMEDIATELY = "START_IMMEDIATELY";

    /** Special parameter for user email. */
    private static final String PARAMETER_USER_EMAIL = "USER_EMAIL";

    /** Special parameter for declaring a user to be in a lite script experiment. */
    static final String PARAMETER_TRIGGER_SCRIPT_EXPERIMENT = "TRIGGER_SCRIPT_EXPERIMENT";

    /**
     * Special parameter for instructing the client to request and run a trigger script prior to
     * starting the regular flow.
     */
    static final String PARAMETER_REQUEST_TRIGGER_SCRIPT = "REQUEST_TRIGGER_SCRIPT";

    /**
     * Special parameter to allow injecting a base64-encoded GetTriggerScriptsResponseProto. When
     * specified, the client will directly use the specified trigger scripts instead of fetching
     * them from a remote backend. Takes precedence over PARAMETER_REQUEST_TRIGGER_SCRIPT.
     */
    static final String PARAMETER_TRIGGER_SCRIPTS_BASE64 = "TRIGGER_SCRIPTS_BASE64";

    /**
     * Identifier used by parameters/or special intent that indicates experiments passed from
     * the caller.
     */
    private static final String PARAMETER_EXPERIMENT_IDS = "EXPERIMENT_IDS";

    /**
     * The original deeplink as indicated by the caller. Use this parameter instead of the
     * initial URL when available to avoid issues where the initial URL points to a redirect
     * rather than the actual deeplink.
     */
    public static final String PARAMETER_ORIGINAL_DEEPLINK = "ORIGINAL_DEEPLINK";

    /** Identifies the caller, i.e., the trigger surface. */
    public static final String PARAMETER_CALLER = "CALLER";

    /**
     * Defines whether or not it is allowed to open an app instead of continuing with Autofill
     * Assistant.
     */
    private static final String PARAMETER_ALLOW_APP = "ALLOW_APP";

    private final Map<String, Object> mScriptParameters;
    private final Map<String, Object> mDeviceOnlyScriptParameters;
    private final StringBuilder mExperimentIds;
    private String mInitialUrl;
    private boolean mOnboardingShown;

    private TriggerContext() {
        mScriptParameters = new HashMap<>();
        mDeviceOnlyScriptParameters = new HashMap<>();
        mExperimentIds = new StringBuilder();
    }

    public static Builder newBuilder() {
        return new Builder(new TriggerContext());
    }

    /**
     * Adds {@code experimentIds} to the list of experiment ids. Ignores empty and null values.
     * Expects the incoming string to be a comma-separated list of experiments.
     */
    private void addExperimentIds(@Nullable String experimentIds) {
        if (TextUtils.isEmpty(experimentIds)) {
            return;
        }
        if (mExperimentIds.length() > 0 && !mExperimentIds.toString().endsWith(",")) {
            mExperimentIds.append(",");
        }
        mExperimentIds.append(experimentIds);
    }

    private boolean getBooleanParameter(String key) {
        Object value = mScriptParameters.get(key);
        if (!(value instanceof Boolean)) { // Also catches null.
            if (value != null) {
                Log.v(TAG, "Expected " + key + " to be boolean, but was " + value.toString());
            }
            return false;
        }

        return (Boolean) value;
    }

    @Nullable
    private String getStringParameter(String key) {
        Object value = mScriptParameters.get(key);
        if (!(value instanceof String)) { // Also catches null.
            if (value != null) {
                Log.v(TAG, "Expected " + key + " to be string, but was " + value.toString());
            }
            return null;
        }

        return decode((String) value);
    }

    public Map<String, String> getParameters() {
        Map<String, String> map = new HashMap<>();

        for (String key : mScriptParameters.keySet()) {
            map.put(key, decode(mScriptParameters.get(key).toString()));
        }

        return map;
    }

    public Map<String, String> getDeviceOnlyParameters() {
        Map<String, String> map = new HashMap<>();

        for (String key : mDeviceOnlyScriptParameters.keySet()) {
            map.put(key, decode(mDeviceOnlyScriptParameters.get(key).toString()));
        }

        return map;
    }

    /**
     * Searches the merged experiment ids in normal and special parameters.
     * @return comma separated list of experiment ids, or empty string.
     */
    public String getExperimentIds() {
        return mExperimentIds.toString();
    }

    /** Returns whether the trigger-script experiment flag is set to true. */
    public boolean isTriggerScriptExperiment() {
        return getBooleanParameter(PARAMETER_TRIGGER_SCRIPT_EXPERIMENT);
    }

    /**
     * Returns the user's email as indicated by the caller, if specified.
     * @return caller's email or null.
     */
    @Nullable
    public String getCallerEmail() {
        return getStringParameter(PARAMETER_USER_EMAIL);
    }

    public String getInitialUrl() {
        return mInitialUrl;
    }

    public String getOriginalDeeplink() {
        return getStringParameter(PARAMETER_ORIGINAL_DEEPLINK);
    }

    /**
     * Returns the startup URL to use.
     */
    public String getStartupUrl() {
        return getOriginalDeeplink() != null ? getOriginalDeeplink() : getInitialUrl();
    }

    /** Whether the caller requests the client to fetch trigger scripts from a remote endpoint. */
    public boolean requestsTriggerScript() {
        return getBooleanParameter(PARAMETER_REQUEST_TRIGGER_SCRIPT);
    }

    /** Whether the caller specified a base64-encoded trigger scripts response or not. */
    public boolean containsBase64TriggerScripts() {
        return !TextUtils.isEmpty(getStringParameter(PARAMETER_TRIGGER_SCRIPTS_BASE64));
    }

    /** Whether the caller requested a trigger script to start in any of the supported ways. */
    public boolean containsTriggerScript() {
        return requestsTriggerScript() || containsBase64TriggerScripts();
    }

    public void setOnboardingShown(boolean onboardingShown) {
        mOnboardingShown = onboardingShown;
    }

    /** Whether the onboarding was shown. TODO(arbesser): move this to native. */
    public boolean getOnboardingShown() {
        return mOnboardingShown;
    }

    /**
     *  Returns true if this trigger context is autofill-assistant-enabled. This does not indicate
     *  that it is definitely possible to start a full flow, it merely signals that the intent is
     *  requesting an autofill-assistant start.
     */
    public boolean isEnabled() {
        return getBooleanParameter(PARAMETER_ENABLED);
    }

    /**
     * Returns true if this trigger context allows for launching an external app instead of
     * continuing with Autofill Assistant.
     */
    public boolean allowAppOverride() {
        return Boolean.parseBoolean(getStringParameter(PARAMETER_ALLOW_APP));
    }

    /** Returns the intent of the flow. */
    public String getIntent() {
        return getStringParameter(PARAMETER_INTENT);
    }
}
