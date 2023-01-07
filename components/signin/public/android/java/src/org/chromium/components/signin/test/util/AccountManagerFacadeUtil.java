// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import org.mockito.Mockito;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

/**
 * Util class to set java AccountManagerFacade for native tests.
 */
final class AccountManagerFacadeUtil {
    /**
     * Stubs AccountManagerFacade for native tests.
     */
    @CalledByNative
    private static void setUpMockFacade() {
        AccountManagerFacadeProvider.setInstanceForTests(Mockito.mock(AccountManagerFacade.class));
    }
}