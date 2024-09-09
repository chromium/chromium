// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;

import java.util.HashMap;

/** Unit tests for {@link AccountInfoServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountInfoServiceImplTest {
    private static final String ACCOUNT_EMAIL = "test@gmail.com";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private AccountInfoService.Observer mObserverMock;

    private final AccountInfo mAccountInfoWithAvatar =
            new AccountInfo(
                    new CoreAccountId("gaia-id-test"),
                    ACCOUNT_EMAIL,
                    "gaia-id-test",
                    "full name",
                    "given name",
                    mock(Bitmap.class),
                    new AccountCapabilities(new HashMap<>()));

    private AccountInfoServiceImpl mService;

    @Before
    public void setUp() {
        AccountInfoServiceProvider.init(mIdentityManagerMock);
        mService = (AccountInfoServiceImpl) AccountInfoServiceProvider.get();
    }

    @After
    public void tearDown() {
        AccountInfoServiceProvider.resetForTests();
    }

    @Test(expected = RuntimeException.class)
    public void testGetInstanceBeforeInitialization() {
        AccountInfoServiceProvider.resetForTests();
        AccountInfoServiceProvider.get();
    }

    @Test
    public void testServiceIsAttachedToIdentityManager() {
        verify(mIdentityManagerMock).addObserver(mService);

        mService.destroy();
        verify(mIdentityManagerMock).removeObserver(mService);
    }

    @Test
    public void testObserverIsNotifiedWhenAdded() {
        mService.addObserver(mObserverMock);

        mService.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
        verify(mObserverMock).onAccountInfoUpdated(mAccountInfoWithAvatar);
    }

    @Test
    public void testObserverIsNotNotifiedAfterRemoval() {
        mService.addObserver(mObserverMock);
        mService.removeObserver(mObserverMock);

        mService.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
        verify(mObserverMock, never()).onAccountInfoUpdated(mAccountInfoWithAvatar);
    }

    @Test
    public void testGetPromiseInvokedBeforeInitialization() {
        AccountInfoServiceProvider.resetForTests();

        final Promise<AccountInfoService> promise = AccountInfoServiceProvider.getPromise();

        Assert.assertFalse(promise.isFulfilled());
        AccountInfoServiceProvider.init(mIdentityManagerMock);
        Assert.assertTrue(promise.isFulfilled());
    }

    @Test
    public void testGetPromiseInvokedAfterInitialization() {
        AccountInfoServiceProvider.resetForTests();
        AccountInfoServiceProvider.init(mIdentityManagerMock);

        final Promise<AccountInfoService> promise = AccountInfoServiceProvider.getPromise();

        Assert.assertTrue(promise.isFulfilled());
    }
}
