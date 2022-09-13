// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.base.Features;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/**
 * Provides an API for querying the status of Autofill Assistant features.
 *
 * TODO(crbug.com/1060097): generate this file.
 */
@JNINamespace("autofill_assistant")
@MainDex
public class AssistantFeatures extends Features {
    public static final String AUTOFILL_ASSISTANT_NAME = "AutofillAssistant";
    public static final String AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME =
            "AutofillAssistantChromeEntry";
    public static final String AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME =
            "AutofillAssistantDirectActions";
    public static final String AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW_NAME =
            "AutofillAssistantDisableOnboardingFlow";
    public static final String AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB_NAME =
            "AutofillAssistantDisableProactiveHelpTiedToMSBB";
    public static final String AUTOFILL_ASSISTANT_FEEDBACK_CHIP_NAME =
            "AutofillAssistantFeedbackChip";
    public static final String AUTOFILL_ASSISTANT_LOAD_DFM_FOR_TRIGGER_SCRIPTS_NAME =
            "AutofillAssistantLoadDFMForTriggerScripts";
    public static final String AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME =
            "AutofillAssistantProactiveHelp";

    // This list must be kept in sync with kFeaturesExposedToJava in features.cc.
    public static final AssistantFeatures AUTOFILL_ASSISTANT =
            new AssistantFeatures(0, AUTOFILL_ASSISTANT_NAME);
    public static final AssistantFeatures AUTOFILL_ASSISTANT_CHROME_ENTRY =
            new AssistantFeatures(1, AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME);
    public static final AssistantFeatures AUTOFILL_ASSISTANT_DIRECT_ACTIONS =
            new AssistantFeatures(2, AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME);
    public static final AssistantFeatures AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW =
            new AssistantFeatures(3, AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW_NAME);
    public static final AssistantFeatures AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB =
            new AssistantFeatures(4, AUTOFILL_ASSISTANT_DISABLE_PROACTIVE_HELP_TIED_TO_MSBB_NAME);
    public static final AssistantFeatures AUTOFILL_ASSISTANT_FEEDBACK_CHIP =
            new AssistantFeatures(5, AUTOFILL_ASSISTANT_FEEDBACK_CHIP_NAME);
    public static final AssistantFeatures AUTOFILL_ASSISTANT_LOAD_DFM_FOR_TRIGGER_SCRIPTS =
            new AssistantFeatures(6, AUTOFILL_ASSISTANT_LOAD_DFM_FOR_TRIGGER_SCRIPTS_NAME);
    public static final AssistantFeatures AUTOFILL_ASSISTANT_PROACTIVE_HELP =
            new AssistantFeatures(7, AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME);

    private final int mOrdinal;

    private AssistantFeatures(int ordinal, String name) {
        super(name);
        mOrdinal = ordinal;
    }

    @Override
    protected long getFeaturePointer() {
        return AssistantFeaturesJni.get().getFeature(mOrdinal);
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }
}
