// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;

/**
 * Unit tests for {@link AccountInfoService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountInfoServiceTest {
    private static final String ACCOUNT_EMAIL = "test@gmail.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private AccountInfoService.Observer mObserverMock;

    private final AccountInfo mAccountInfoWithAvatar =
            new AccountInfo(new CoreAccountId("gaia-id-test"), ACCOUNT_EMAIL, "gaia-id-test",
                    "full name", "given name", mock(Bitmap.class));

    private AccountInfoService mService;

    @Before
    public void setUp() {
        AccountInfoService.init(mIdentityManagerMock);
        mService = AccountInfoService.get();
    }

    @After
    public void tearDown() {
        AccountInfoService.resetForTests();
    }

    @Test(expected = RuntimeException.class)
    public void testGetInstanceBeforeInitialization() {
        AccountInfoService.resetForTests();
        AccountInfoService.get();
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
}
