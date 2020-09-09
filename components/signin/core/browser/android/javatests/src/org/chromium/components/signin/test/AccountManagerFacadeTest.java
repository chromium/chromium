// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacadeImpl;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

import java.util.concurrent.CountDownLatch;

/**
 * Tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeRobolectricTest}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccountManagerFacadeTest {
    private final FakeAccountManagerDelegate mDelegate =
            new FakeAccountManagerDelegate(FakeAccountManagerDelegate.ENABLE_BLOCK_GET_ACCOUNTS);

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacadeProvider.setInstanceForTests(
                    new AccountManagerFacadeImpl(mDelegate));
        });
    }

    @After
    public void tearDown() {
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    @Test
    @SmallTest
    public void testIsCachePopulated() throws AccountManagerDelegateException {
        // Cache shouldn't be populated until getAccountsSync is unblocked.
        assertFalse(AccountManagerFacadeProvider.getInstance().isCachePopulated());

        mDelegate.unblockGetAccounts();
        // Wait for cache population to finish.
        AccountManagerFacadeProvider.getInstance().getGoogleAccounts();
        assertTrue(AccountManagerFacadeProvider.getInstance().isCachePopulated());
    }

    @Test
    @SmallTest
    public void testRunAfterCacheIsPopulated() throws InterruptedException {
        CountDownLatch firstCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Add callback. This should be done on the main thread.
            AccountManagerFacadeProvider.getInstance().runAfterCacheIsPopulated(
                    firstCounter::countDown);
        });
        assertEquals("Callback shouldn't be invoked until cache is populated", 1,
                firstCounter.getCount());

        mDelegate.unblockGetAccounts();
        // Cache should be populated & callback should be invoked
        firstCounter.await();

        CountDownLatch secondCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacadeProvider.getInstance().runAfterCacheIsPopulated(
                    secondCounter::countDown);
            assertEquals("Callback should be posted on UI thread, not executed synchronously", 1,
                    secondCounter.getCount());
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertEquals(
                "Callback should be posted to UI thread right away", 0, secondCounter.getCount());
    }
}
