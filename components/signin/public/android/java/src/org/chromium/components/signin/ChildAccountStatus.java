// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Status of an account regarding being a child.
 *
 * An account can be in one of three states in this regard:
 * <ul>
 *   <li>Not child: the account doesn't belong to a child.</li>
 *   <li>Child account, either one of
 *     <ul>
 *       <li>Regular child accounts, marked with an "uca" flag.</li>
 *       <li>
 *         USM child accounts, marked with an "usm" flag. See http://go/chrome-usm-accounts for more
 *         details.
 *       </li>
 *     </ul>
 *   <li>
 * </ul>
 */
public final class ChildAccountStatus {
    private ChildAccountStatus() {}

    // Not a child account.
    public static final int NOT_CHILD = 0;
    // Regular child account, with a "uca" service flag.
    public static final int REGULAR_CHILD = 1;
    // USM child account, with a "usm" service flag.
    public static final int USM_CHILD = 2;

    /** Returns whether the status corresponds to a child account. */
    public static boolean isChild(@Status int status) {
        return status != NOT_CHILD;
    }

    @IntDef({NOT_CHILD, REGULAR_CHILD, USM_CHILD})
    @Retention(RetentionPolicy.SOURCE)
    /** Status of an account regarding child type. */
    public @interface Status {}
}
