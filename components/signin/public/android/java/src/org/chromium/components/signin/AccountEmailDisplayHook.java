// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.chromium.base.ServiceLoaderUtil;

/** Displayability of email addresses in Chrome UI based on the email domain. */
public interface AccountEmailDisplayHook {
    boolean canHaveEmailAddressDisplayedInternal(String email);

    /**
     * Check if the user's email address can be displayed based on the email domain.
     *
     * <p>This method is used in place of {@link
     * org.chromium.components.signin.base.AccountCapabilities#canHaveEmailAddressDisplayed()} when
     * the capability or {@link org.chromium.components.signin.base.AccountInfo} is not available.
     */
    static boolean canHaveEmailAddressDisplayed(String email) {
        AccountEmailDisplayHook impl = ServiceLoaderUtil.maybeCreate(AccountEmailDisplayHook.class);
        return impl == null || impl.canHaveEmailAddressDisplayedInternal(email);
    }
}
