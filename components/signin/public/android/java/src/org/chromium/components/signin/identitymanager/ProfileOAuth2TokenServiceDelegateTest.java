// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;

/** Tests for {@link ProfileOAuth2TokenServiceDelegate}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ProfileOAuth2TokenServiceDelegateTest {
    private static final long NATIVE_DELEGATE = 1000L;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ProfileOAuth2TokenServiceDelegate.Natives mNativeMock;

    private final FakeAccountManagerFacade mAccountManagerFacade = new FakeAccountManagerFacade();

    private ProfileOAuth2TokenServiceDelegate mDelegate;

    @Before
    public void setUp() {
        ProfileOAuth2TokenServiceDelegateJni.setInstanceForTesting(mNativeMock);
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
        mDelegate = new ProfileOAuth2TokenServiceDelegate(NATIVE_DELEGATE);
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsNotOnDevice() {
        mAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mDelegate.hasOAuth2RefreshToken("test2@gmail.com"));
                });
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsOnDevice() {
        mAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            mDelegate.hasOAuth2RefreshToken(TestAccounts.ACCOUNT1.getEmail()));
                });
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenCacheIsNotPopulated() {
        try (var block =
                mAccountManagerFacade.blockGetCoreAccountInfos(/* populateCache= */ false)) {
            mAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Assert.assertFalse(
                                mDelegate.hasOAuth2RefreshToken(TestAccounts.ACCOUNT1.getEmail()));
                    });
        }
    }
}
