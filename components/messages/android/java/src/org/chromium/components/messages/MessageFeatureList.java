// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.messages.MessageFeatureMap.AccessibilityEventInvestigationGroup;

/**
 * Lists base::Features that can be accessed through {@link MessageFeatureMap}.
 *
 * <p>Should be kept in sync with |kFeaturesExposedToJava| in
 * //components/messages/android/messages_feature.cc
 */
@NullMarked
public abstract class MessageFeatureList {
    public static final String MESSAGES_ACCESSIBILITY_EVENT_INVESTIGATIONS =
            "MessagesAccessibilityEventInvestigations";
    public static final String MESSAGES_FOR_ANDROID_FULLY_VISIBLE_CALLBACK =
            "MessagesForAndroidFullyVisibleCallback";
    public static final String MESSAGES_ANDROID_EXTRA_HISTOGRAMS = "MessagesAndroidExtraHistograms";
    public static final String MESSAGES_CLOSE_BUTTON = "MessagesCloseButton";

    public static @AccessibilityEventInvestigationGroup int
            getMessagesAccessibilityEventInvestigationsParam() {
        return getFieldTrialParamByFeatureAsInt(
                MESSAGES_ACCESSIBILITY_EVENT_INVESTIGATIONS,
                "messages_accessibility_events_investigations_param",
                AccessibilityEventInvestigationGroup.DEFAULT);
    }

    public static boolean isMessagesAccessibilityEventInvestigationsEnabled() {
        return MessageFeatureMap.isEnabled(MESSAGES_ACCESSIBILITY_EVENT_INVESTIGATIONS);
    }

    public static boolean isFullyVisibleCallbackEnabled() {
        return MessageFeatureMap.isEnabled(MESSAGES_FOR_ANDROID_FULLY_VISIBLE_CALLBACK);
    }

    public static boolean areExtraHistogramsEnabled() {
        return MessageFeatureMap.isEnabled(MESSAGES_ANDROID_EXTRA_HISTOGRAMS);
    }

    public static boolean isCloseButtonEnabled() {
        return MessageFeatureMap.isEnabled(MESSAGES_CLOSE_BUTTON);
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The integer value to use if the param is not available.
     * @return The parameter value as an int. Default value if the feature does not exist or the
     *     specified parameter does not exist or its string value does not represent an int.
     */
    private static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        return MessageFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsInt(featureName, paramName, defaultValue);
    }
}
