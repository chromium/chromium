// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;

/** Autofill Assistant related preferences util class. */
// TODO(crbug.com/1069897): Use SharedPreferencesManager again.
public class AutofillAssistantPreferencesUtil {
    /** Whether Autofill Assistant is enabled */
    private static final String ENABLED_PREFERENCE_KEY = "autofill_assistant_switch";
    /** Whether the Autofill Assistant onboarding has been accepted. */
    private static final String ONBOARDING_ACCEPTED_PREFERENCE_KEY =
            "AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED";
    /** Whether the user has seen a lite-script before or is a first-time user. */
    private static final String FIRST_TIME_LITE_SCRIPT_USER_PREFERENCE_KEY =
            "Chrome.AutofillAssistant.LiteScriptFirstTimeUser";
    /** Whether proactive help is enabled. */
    private static final String PROACTIVE_HELP_PREFERENCE_KEY =
            "Chrome.AutofillAssistant.ProactiveHelp";
    /**
     * @Deprecated preference indicating whether "do not show again" was checked in the autofill
     * assistant onboarding
     */
    @Deprecated
    private static final String SKIP_INIT_SCREEN_PREFERENCE_KEY =
            "AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN";

    /** Checks whether the Autofill Assistant switch preference in settings is on. */
    public static boolean isAutofillAssistantSwitchOn() {
        return getAssistantEnabledPreference(true);
    }

    /** Checks whether proactive help is enabled. */
    public static boolean isProactiveHelpOn() {
        return isProactiveHelpSwitchOn() && isAutofillAssistantSwitchOn();
    }

    /**
     * Checks whether the proactive help switch preference in settings is on.
     * Warning: even if the switch is on, it can appear disabled if the Autofill Assistant switch is
     * off. Use {@link #isProactiveHelpOn()} to determine whether to trigger proactive help.
     */
    private static boolean isProactiveHelpSwitchOn() {
        if (!AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP.isEnabled()) {
            return false;
        }

        return getProactiveHelpPreference(true);
    }

    /** Checks whether the Autofill Assistant onboarding has been accepted. */
    public static boolean isAutofillOnboardingAccepted() {
        return getOnboardingAcceptedPreference(false) ||
                /* Legacy treatment: users of earlier versions should not have to see the onboarding
                again if they checked the `do not show again' checkbox*/
                getSkipInitScreenPreference(false);
    }

    /** Checks whether the Autofill Assistant onboarding screen should be shown. */
    public static boolean getShowOnboarding() {
        if (AssistantFeatures.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW.isEnabled()) {
            return false;
        }
        return !isAutofillAssistantSwitchOn() || !isAutofillOnboardingAccepted();
    }

    /**
     * Sets preferences from the initial screen.
     *
     * @param accept Flag indicating whether the ToS have been accepted.
     */
    public static void setInitialPreferences(boolean accept) {
        if (accept) {
            setAssistantEnabledPreference(accept);
        }
        setOnboardingAcceptedPreference(accept);
    }

    public static boolean getAssistantEnabledPreference(boolean defaultValue) {
        return readBoolean(ENABLED_PREFERENCE_KEY, defaultValue);
    }

    public static void setAssistantEnabledPreference(boolean value) {
        writeBoolean(ENABLED_PREFERENCE_KEY, value);
    }

    public static boolean containsAssistantEnabledPreference() {
        return contains(ENABLED_PREFERENCE_KEY);
    }

    public static boolean getOnboardingAcceptedPreference(boolean defaultValue) {
        return readBoolean(ONBOARDING_ACCEPTED_PREFERENCE_KEY, defaultValue);
    }

    public static void setOnboardingAcceptedPreference(boolean value) {
        writeBoolean(ONBOARDING_ACCEPTED_PREFERENCE_KEY, value);
    }

    public static void removeOnboardingAcceptedPreference() {
        remove(ONBOARDING_ACCEPTED_PREFERENCE_KEY);
    }

    /** Returns whether the user has seen a trigger script before or not. */
    public static boolean isAutofillAssistantFirstTimeTriggerScriptUser() {
        return readBoolean(FIRST_TIME_LITE_SCRIPT_USER_PREFERENCE_KEY, true);
    }

    /** Marks a user as having seen a trigger script at least once before. */
    public static void setFirstTimeTriggerScriptUserPreference(boolean firstTimeUser) {
        writeBoolean(FIRST_TIME_LITE_SCRIPT_USER_PREFERENCE_KEY, firstTimeUser);
    }

    public static void onClearBrowserHistory() {
        remove(FIRST_TIME_LITE_SCRIPT_USER_PREFERENCE_KEY);
    }

    public static boolean getProactiveHelpPreference(boolean defaultValue) {
        return readBoolean(PROACTIVE_HELP_PREFERENCE_KEY, defaultValue);
    }

    /** Enables or disables the proactive help setting. */
    public static void setProactiveHelpPreference(boolean enabled) {
        writeBoolean(PROACTIVE_HELP_PREFERENCE_KEY, enabled);
    }

    public static boolean getSkipInitScreenPreference(boolean defaultValue) {
        return readBoolean(SKIP_INIT_SCREEN_PREFERENCE_KEY, defaultValue);
    }

    public static void removeSkipInitScreenPreference() {
        remove(SKIP_INIT_SCREEN_PREFERENCE_KEY);
    }

    private static boolean readBoolean(String key, boolean defaultValue) {
        return ContextUtils.getAppSharedPreferences().getBoolean(key, defaultValue);
    }

    private static void writeBoolean(String key, boolean value) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putBoolean(key, value);
        ed.apply();
    }

    private static boolean contains(String key) {
        return ContextUtils.getAppSharedPreferences().contains(key);
    }

    private static void remove(String key) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.remove(key);
        ed.apply();
    }

    // Avoid instantiation by accident.
    private AutofillAssistantPreferencesUtil() {}
}
