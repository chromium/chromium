// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

import java.util.concurrent.CountDownLatch;

/**
 * Tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeRobolectricTest}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccountManagerFacadeTest {
    private FakeAccountManagerDelegate mDelegate =
            new FakeAccountManagerDelegate(FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE,
                    FakeAccountManagerDelegate.ENABLE_BLOCK_GET_ACCOUNTS);

    @Before
    public void setUp() {
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mDelegate);
    }

    @Test
    @SmallTest
    public void testIsCachePopulated() throws AccountManagerDelegateException {
        // Cache shouldn't be populated until getAccountsSync is unblocked.
        assertFalse(AccountManagerFacade.get().isCachePopulated());

        mDelegate.unblockGetAccounts();
        // Wait for cache population to finish.
        AccountManagerFacade.get().getGoogleAccounts();
        assertTrue(AccountManagerFacade.get().isCachePopulated());
    }

    @Test
    @SmallTest
    public void testRunAfterCacheIsPopulated() throws InterruptedException {
        CountDownLatch firstCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Add callback. This should be done on the main thread.
            AccountManagerFacade.get().runAfterCacheIsPopulated(firstCounter::countDown);
        });
        assertEquals("Callback shouldn't be invoked until cache is populated", 1,
                firstCounter.getCount());

        mDelegate.unblockGetAccounts();
        // Cache should be populated & callback should be invoked
        firstCounter.await();

        CountDownLatch secondCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacade.get().runAfterCacheIsPopulated(secondCounter::countDown);
            assertEquals("Callback should be posted on UI thread, not executed synchronously", 1,
                    secondCounter.getCount());
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertEquals(
                "Callback should be posted to UI thread right away", 0, secondCounter.getCount());
    }
}
