// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

/**
 * AccountManagerDelegateException encapsulates errors that can happen while getting list of
 * accounts.
 */
public class AccountManagerDelegateException extends Exception {
    public AccountManagerDelegateException(String message) {
        super(message);
    }
}
