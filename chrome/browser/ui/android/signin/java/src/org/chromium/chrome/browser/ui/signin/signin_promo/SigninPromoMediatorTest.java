// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
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
        verify(mProfileDataCache, atLeastOnce())
                .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail());

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        verify(mProfileDataCache).getProfileDataOrDefault(TestAccounts.ACCOUNT2.getEmail());
    }

    @Test
    public void testDelegateUpdated_defaultAccountRemoved() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createSigninPromoMediator();

        // Set the mock delegate to return non-default values.
        String newTitle = "newTitle";
        String newDescription = "newDescription";
        String newPrimaryButtonText = "newPrimaryButtonText";
        String newSecondaryButtonText = "newSecondaryButtonText";
        doReturn(true).when(mDelegate).refreshPromoState(any());
        doReturn(true).when(mDelegate).shouldHideDismissButton();
        doReturn(newTitle).when(mDelegate).getTitle();
        doReturn(newDescription).when(mDelegate).getDescription();
        doReturn(newPrimaryButtonText).when(mDelegate).getTextForPrimaryButton(any());
        doReturn(newSecondaryButtonText).when(mDelegate).getTextForSecondaryButton();
        // Remove the default account to trigger a promo content refresh.
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        // Verify that the promo's model uses the new values returned by the delegate.
        boolean shouldHideDismissButton =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON);
        String title = mMediator.getModel().get(SigninPromoProperties.TITLE_TEXT);
        String description = mMediator.getModel().get(SigninPromoProperties.DESCRIPTION_TEXT);
        String primaryButtonText =
                mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        String secondaryButtonText =
                mMediator.getModel().get(SigninPromoProperties.SECONDARY_BUTTON_TEXT);
        assertTrue(shouldHideDismissButton);
        assertEquals(newTitle, title);
        assertEquals(newDescription, description);
        assertEquals(newPrimaryButtonText, primaryButtonText);
        assertEquals(newSecondaryButtonText, secondaryButtonText);
    }

    private void createSigninPromoMediator() {
        Context context = ApplicationProvider.getApplicationContext();
        mProfileDataCache = spy(ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context));
        doReturn(true).when(mDelegate).canShowPromo();
        mMediator =
                new SigninPromoMediator(
                        mIdentityManager,
                        mSyncService,
                        AccountManagerFacadeProvider.getInstance(),
                        mProfileDataCache,
                        mDelegate);
    }
}
