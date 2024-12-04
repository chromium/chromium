// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;

@RunWith(BaseRobolectricTestRunner.class)
public class SigninPromoMediatorTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    // TODO(crbug.com/374683682): Create and use FakeIdentityManager and add tests for sign-in and
    // sign-out events.
    private @Mock IdentityManager mIdentityManager;
    private @Mock SyncService mSyncService;
    private @Mock SigninPromoDelegate mDelegate;
    private ProfileDataCache mProfileDataCache;

    private SigninPromoMediator mMediator;

    @Test
    public void testSecondaryButtonHiddenByDelegate() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mDelegate).shouldHideSecondaryButton();
        doReturn(TestAccounts.ACCOUNT1)
                .when(mIdentityManager)
                .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        createSigninPromoMediator();

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertTrue(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonHiddenByNullProfileData() {
        createSigninPromoMediator();

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertTrue(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonShown_visibleAccountFromIdentityManager() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(false).when(mDelegate).shouldHideSecondaryButton();
        doReturn(TestAccounts.ACCOUNT1)
                .when(mIdentityManager)
                .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        createSigninPromoMediator();

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertFalse(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonShown_visibleAccountFromAccountManager() {
        doReturn(false).when(mDelegate).shouldHideSecondaryButton();
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        createSigninPromoMediator();

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertFalse(isSecondaryButtonHidden);
    }

    @Test
    public void testDefaultAccountRemoved() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        createSigninPromoMediator();
        verify(mProfileDataCache).getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail());

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        verify(mProfileDataCache).getProfileDataOrDefault(TestAccounts.ACCOUNT2.getEmail());
    }

    private void createSigninPromoMediator() {
        Context context = ApplicationProvider.getApplicationContext();
        mProfileDataCache = spy(ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context));
        mMediator =
                new SigninPromoMediator(
                        mIdentityManager,
                        mSyncService,
                        AccountManagerFacadeProvider.getInstance(),
                        mProfileDataCache,
                        mDelegate);
    }
}
