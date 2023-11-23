// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Event class for primary account change events fired by {@link
 * org.chromium.components.signin.identitymanager.IdentityManager} This class has a native
 * counterpart called PrimaryAccountChangeEvent.
 */
public class PrimaryAccountChangeEvent {
    /**
     * This class mirrors the native PrimaryAccountChangeEvent class Type enum from:
     * components/signin/public/identity_manager/primary_account_change_event.h
     */
    @IntDef({Type.NONE, Type.SET, Type.CLEARED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // No change.
        int NONE = 0;
        // Primary account set or changed.
        int SET = 1;
        // Primary account cleared.
        int CLEARED = 2;
    }

    private final @Type int mEventTypeForConsentLevelSync;
    private final @Type int mEventTypeForConsentLevelNotRequired;

    @CalledByNative
    @VisibleForTesting
    public PrimaryAccountChangeEvent(
            @Type int eventTypeForConsentLevelNotRequired, @Type int eventTypeForConsentLevelSync) {
        mEventTypeForConsentLevelNotRequired = eventTypeForConsentLevelNotRequired;
        mEventTypeForConsentLevelSync = eventTypeForConsentLevelSync;
        assert mEventTypeForConsentLevelNotRequired != Type.NONE
                        || mEventTypeForConsentLevelSync != Type.NONE
                : "PrimaryAccountChangeEvent should not be fired for no-change events";
    }

    /**
     * Returns primary account change event type for the corresponding consentLevel.
     * @param consentLevel The consent level for the primary account change.
     * @return The event type for the change.
     *         NONE - No change in primary account for consentLevel.
     *         SET - A new primary account is set or changed for consentLevel.
     *         CLEARED - The primary account set for consentLevel is cleared.
     */
    public @Type int getEventTypeFor(@ConsentLevel int consentLevel) {
        return consentLevel == ConsentLevel.SYNC
                ? mEventTypeForConsentLevelSync
                : mEventTypeForConsentLevelNotRequired;
    }
}
