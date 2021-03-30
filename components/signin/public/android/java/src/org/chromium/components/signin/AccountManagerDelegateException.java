// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

/**
 * AccountManagerDelegateException encapsulates errors that can happen while getting list of
 * accounts.
 */
public abstract class AccountManagerDelegateException extends Exception {
    protected AccountManagerDelegateException(String message) {
        super(message);
    }

    protected AccountManagerDelegateException(AccountManagerDelegateException cause) {
        super(cause);
    }
}
