// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Registry of features explicitly approved as "User Critical".
 *
 * <p>A sheet should ONLY be registered here if interrupting or suppressing it causes:
 *
 * <ul>
 *   <li>Security or authentication failures (e.g., Passkeys, WebAuthn).
 *   <li>Irreversible data loss or credential exposure.
 *   <li>Critical transaction disruption (e.g., Payments autofill).
 * </ul>
 *
 * <p><strong>ANY MODIFICATIONS TO THIS FILE REQUIRE BOTTOMSHEET OWNERS APPROVAL.</strong>
 */
@NullMarked
@IntDef({
    UserCriticalFeature.NONE,
    UserCriticalFeature.TEST,
    UserCriticalFeature.TOUCH_TO_FILL_PASSWORD_MANAGER,
    UserCriticalFeature.ALL_PASSWORDS,
    UserCriticalFeature.TOUCH_TO_FILL_PASSWORD_GENERATION,
    UserCriticalFeature.TOUCH_TO_FILL_NO_PASSKEYS,
    UserCriticalFeature.ACKNOWLEDGE_GROUPED_CREDENTIAL,
    UserCriticalFeature.MANDATORY_REAUTH_OPT_IN,
    UserCriticalFeature.AUTHENTICATOR_INCOGNITO_CONFIRMATION,
    UserCriticalFeature.PAYMENT_HANDLER,
    UserCriticalFeature.SECURE_PAYMENT_CONFIRMATION,
    UserCriticalFeature.TOUCH_TO_FILL_PAYMENT_METHOD,
})
@Retention(RetentionPolicy.SOURCE)
public @interface UserCriticalFeature {
    int NONE = 0;
    int TEST = 1;
    int TOUCH_TO_FILL_PASSWORD_MANAGER = 2;
    int ALL_PASSWORDS = 3;
    int TOUCH_TO_FILL_PASSWORD_GENERATION = 4;
    int TOUCH_TO_FILL_NO_PASSKEYS = 5;
    int ACKNOWLEDGE_GROUPED_CREDENTIAL = 6;
    int MANDATORY_REAUTH_OPT_IN = 7;
    int AUTHENTICATOR_INCOGNITO_CONFIRMATION = 8;
    int PAYMENT_HANDLER = 9;
    int SECURE_PAYMENT_CONFIRMATION = 10;
    int TOUCH_TO_FILL_PAYMENT_METHOD = 11;
}
