// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

/**
 * Displayability of email addresses in Chrome UI based on the email domain.
 *
 * The correct version of this class will be determined at compile time via build rules.
 */
public class AccountEmailDomainDisplayability {
    /**
     * Check if the user's email address can be displayed based on the email domain.
     *
     * This method is used in place of
     * {@link
     * org.chromium.components.signin.base.AccountCapabilities#canHaveEmailAddressDisplayed()} when
     * the capability or {@link org.chromium.components.signin.base.AccountInfo} is not available.
     */
    public static boolean checkIfDisplayableEmailAddress(String email) {
        return true;
    }
}
