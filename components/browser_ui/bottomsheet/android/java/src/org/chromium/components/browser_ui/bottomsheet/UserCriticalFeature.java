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
})
@Retention(RetentionPolicy.SOURCE)
public @interface UserCriticalFeature {
    int NONE = 0;
    int TEST = 1;
}
