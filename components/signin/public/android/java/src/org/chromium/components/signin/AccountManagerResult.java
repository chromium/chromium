// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.Nullable;

/**
 * AccountManagerResult encapsulates result of {@link AccountManagerFacade} method call. It is a
 * holder that contains either a value or an AccountManagerDelegateException that occurred during
 * the call.
 *
 * @param <T> The type of value this class should contain.
 */
public class AccountManagerResult<T> {
    /**
     * Two possible states of AccountManagerResult are distinguished by mException field. If
     * mException is null, then this instance is in 'value' state. If mException is non-null,
     * then this instance is in 'exception' state. Please note that mException and mValue can both
     * be null - it corresponds to 'value' state with the value being null. Both mValue and
     * mException can't be non-null, though: this invariant is enforced by constructors.
     */
    @Nullable
    private final T mValue;
    @Nullable
    private final AccountManagerDelegateException mException;

    public AccountManagerResult(@Nullable T value) {
        mValue = value;
        mException = null;
    }

    public AccountManagerResult(AccountManagerDelegateException ex) {
        assert ex != null;
        mValue = null;
        mException = ex;
    }

    @Nullable
    public T get() throws AccountManagerDelegateException {
        if (mException != null) {
            throw mException;
        }
        return mValue;
    }

    public boolean hasValue() {
        return mException == null;
    }

    public boolean hasException() {
        return mException != null;
    }

    @Nullable
    public T getValue() {
        assert hasValue();
        return mValue;
    }

    public AccountManagerDelegateException getException() {
        assert hasException();
        return mException;
    }
}
