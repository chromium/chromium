// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import static org.mockito.Mockito.doReturn;

import android.accounts.Account;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

import java.util.List;

/** Tests for {@link ProfileOAuth2TokenServiceDelegate}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ProfileOAuth2TokenServiceDelegateTest {
    private static final long NATIVE_DELEGATE = 1000L;
    private static final String EMAIL = "test@gmail.com";
    private static final CoreAccountInfo CORE_ACCOUNT_INFO =
            CoreAccountInfo.createFromEmailAndGaiaId(
                    EMAIL, FakeAccountManagerFacade.toGaiaId(EMAIL));
    private static final Account ACCOUNT =
            AccountUtils.createAccountFromName(CORE_ACCOUNT_INFO.getEmail());

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final JniMocker mocker = new JniMocker();

    @Mock private ProfileOAuth2TokenServiceDelegate.Natives mNativeMock;

    @Spy
    private final FakeAccountManagerFacade mAccountManagerFacade = new FakeAccountManagerFacade();

    private ProfileOAuth2TokenServiceDelegate mDelegate;

    @Before
    public void setUp() {
        mocker.mock(ProfileOAuth2TokenServiceDelegateJni.TEST_HOOKS, mNativeMock);
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
        mDelegate = new ProfileOAuth2TokenServiceDelegate(NATIVE_DELEGATE);
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsNotOnDevice() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mDelegate.hasOAuth2RefreshToken("test2@gmail.com"));
                });
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsOnDevice() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mDelegate.hasOAuth2RefreshToken(ACCOUNT.name));
                });
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenCacheIsNotPopulated() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    doReturn(new Promise<List<CoreAccountInfo>>())
                            .when(mAccountManagerFacade)
                            .getCoreAccountInfos();
                    Assert.assertFalse(mDelegate.hasOAuth2RefreshToken(ACCOUNT.name));
                });
    }
}
