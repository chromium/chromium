// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import org.jni_zero.CalledByNative;
import org.mockito.Mockito;

import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

/** Util class to set java AccountManagerFacade for native tests. */
final class AccountManagerFacadeUtil {
    /** Stubs AccountManagerFacade for native tests. */
    @CalledByNative
    private static void setUpMockFacade(boolean useFakeImpl) {
        // TODO(crbug.com/40276391): Remove Mockito and use FakeAccountManagerFacade instead.
        AccountManagerFacadeProvider.setInstanceForTests(
                useFakeImpl
                        ? new FakeAccountManagerFacade()
                        : Mockito.mock(AccountManagerFacade.class));
    }
}
