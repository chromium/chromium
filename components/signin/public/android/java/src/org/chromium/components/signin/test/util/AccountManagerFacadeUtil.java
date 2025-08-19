// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import org.jni_zero.CalledByNative;

import org.chromium.components.signin.AccountManagerFacadeProvider;

/** Util class to set java AccountManagerFacade for native tests. */
final class AccountManagerFacadeUtil {
    /** Stubs AccountManagerFacade for native tests. */
    @CalledByNative
    private static void setUpFakeFacade() {
        AccountManagerFacadeProvider.setInstanceForTests(new FakeAccountManagerFacade());
    }
}
