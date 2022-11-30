// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.components.prefs.PrefService;

/** A thin wrapper around the prefs needed for Autofill Assistant. */
// TODO(crbug.com/1362553): Eliminate once there is no more code in
// components/autofill_assistant that needs to access prefs directly.
public class AutofillAssistantPreferenceManager {
    // NOTE: These entries must be kept in sync with those in
    // components/autofill_assistant/browser/public/prefs.h.
    static final String AUTOFILL_ASSISTANT_CONSENT = "autofill_assistant.consent";
    static final String AUTOFILL_ASSISTANT_ENABLED = "autofill_assistant.enabled";

    private final PrefService mPrefService;

    public AutofillAssistantPreferenceManager(PrefService prefService) {
        mPrefService = prefService;
    }

    /** Sets the necessary prefs after an interaction with an onboarding dialog. */
    public void setOnboardingAccepted(boolean accept) {
        if (accept) {
            mPrefService.setBoolean(AUTOFILL_ASSISTANT_ENABLED, true);
        }
        mPrefService.setBoolean(AUTOFILL_ASSISTANT_CONSENT, accept);
    }

    /** Returns whether the onboarding dialog has been accepted. */
    public boolean getOnboardingConsent() {
        return mPrefService.getBoolean(AUTOFILL_ASSISTANT_CONSENT);
    }
}
