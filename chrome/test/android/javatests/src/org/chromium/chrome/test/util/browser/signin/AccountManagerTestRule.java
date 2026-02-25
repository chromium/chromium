// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import androidx.annotation.Nullable;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.google_apis.gaia.CoreAccountId;

/**
 * Test rule establishing a simulated account management environment for unit tests, using a {@link
 * FakeAccountManagerFacade} and a {@link FakeIdentityManager}.
 *
 * <p>The rule will not invoke any native code, therefore it is safe to use it in Robolectric tests.
 */
public class AccountManagerTestRule implements TestRule {
    private final FakeAccountManagerFacade mFakeAccountManagerFacade;
    private final FakeIdentityManager mFakeIdentityManager;

    public AccountManagerTestRule() {
        this(new FakeAccountManagerFacade(), new FakeIdentityManager());
    }

    public AccountManagerTestRule(FakeAccountManagerFacade fakeAccountManagerFacade) {
        this(fakeAccountManagerFacade, new FakeIdentityManager());
    }

    public AccountManagerTestRule(
            FakeAccountManagerFacade fakeAccountManagerFacade,
            FakeIdentityManager fakeIdentityManager) {
        mFakeAccountManagerFacade = fakeAccountManagerFacade;
        mFakeIdentityManager = fakeIdentityManager;
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUpRule();
                statement.evaluate();
            }
        };
    }

    /** Sets up the FakeIdentityManager and FakeAccountManagerFacade mocks. */
    private void setUpRule() {
        IdentityServicesProvider.setIdentityManagerForTesting(mFakeIdentityManager);
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    /** Returns the {@link FakeAccountManagerFacade} used by this test rule. */
    public FakeAccountManagerFacade getAccountManagerFacade() {
        return mFakeAccountManagerFacade;
    }

    /** Returns the {@link FakeIdentityManager} used by this test rule. */
    public FakeIdentityManager getIdentityManager() {
        return mFakeIdentityManager;
    }

    /** Adds an account to the {@link FakeAccountManagerFacade} and {@link FakeIdentityManager}. */
    public void addAccount(AccountInfo accountInfo) {
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(accountInfo);
        mFakeAccountManagerFacade.addAccount(accountInfo);
    }

    /**
     * Updates an account in the {@link FakeAccountManagerFacade} and {@link FakeIdentityManager}.
     */
    public void updateAccount(AccountInfo accountInfo) {
        mFakeAccountManagerFacade.updateAccount(accountInfo);
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(accountInfo);
    }

    /**
     * Initializes the next add account flow with a given account to add.
     *
     * @param newAccount The account that should be added by the add account flow.
     */
    public void setAddAccountFlowResult(@Nullable AccountInfo newAccount) {
        mFakeAccountManagerFacade.setAddAccountFlowResult(newAccount);
    }

    /** Removes an account with the given {@link CoreAccountId}. */
    public void removeAccount(CoreAccountId accountId) {
        mFakeAccountManagerFacade.removeAccount(accountId);
        mFakeIdentityManager.removeAccount(accountId);
    }

    public void setAccountFetchFailed() {
        mFakeAccountManagerFacade.setAccountFetchFailed();
    }

    /**
     * Block updates from {@link FakeAccountManagerFacade}. See {@link
     * FakeAccountManagerFacade#blockGetAccounts()}.
     */
    public FakeAccountManagerFacade.UpdateBlocker blockGetAccountsUpdate() {
        return mFakeAccountManagerFacade.blockGetAccounts();
    }

    /**
     * Block updates from {@link FakeAccountManagerFacade} and populates the AccountManagerFacade
     * with the currently available accounts. See {@link
     * FakeAccountManagerFacade#blockGetAccountsAndPopulateCache()}.
     */
    public FakeAccountManagerFacade.UpdateBlocker blockGetAccountsUpdateAndPopulateCache() {
        return mFakeAccountManagerFacade.blockGetAccountsAndPopulateCache();
    }

    /**
     * Block updates from {@link FakeIdentityManager}. See {@link
     * FakeIdentityManager#blockExtendedAccountInfoUpdate(boolean)}.
     */
    public void blockExtendedAccountInfoUpdate() {
        mFakeIdentityManager.blockExtendedAccountInfoUpdate();
    }
}
