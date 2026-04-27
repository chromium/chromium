// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Event class for primary account change events fired by {@link
 * org.chromium.components.signin.identitymanager.IdentityManager} This class has a native
 * counterpart called PrimaryAccountChangeEvent.
 */
@NullMarked
@JNINamespace("signin")
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

    private final @Type int mEventType;

    @CalledByNative
    @VisibleForTesting
    public PrimaryAccountChangeEvent(@Type int eventType) {
        mEventType = eventType;
        assert mEventType != Type.NONE
                : "PrimaryAccountChangeEvent should not be fired for no-change events";
    }

    /**
     * Returns primary account change event type.
     *
     * @return The event type for the change:
     *     <ul>
     *       <li>NONE - No change in primary account.
     *       <li>SET - A new primary account is set or changed.
     *       <li>CLEARED - The primary account set is cleared.
     *     </ul>
     */
    public @Type int getEventTypeFor() {
        return mEventType;
    }
}
