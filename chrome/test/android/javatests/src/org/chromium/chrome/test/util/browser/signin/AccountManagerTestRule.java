// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.google_apis.gaia.CoreAccountId;

/**
 * This test rule establishing a simulated account management environment for unit tests, using a
 * FakeAccountManagerFacade and a FakeAccountInfoService.
 *
 * <p>The rule will not invoke any native code, therefore it is safe to use it in Robolectric tests.
 */
public class AccountManagerTestRule implements TestRule {
    private final @NonNull FakeAccountManagerFacade mFakeAccountManagerFacade;
    // TODO(crbug.com/40234741): Revise this test rule and make this non-nullable.
    private final @Nullable FakeAccountInfoService mFakeAccountInfoService;

    public AccountManagerTestRule() {
        this(new FakeAccountManagerFacade(), new FakeAccountInfoService());
    }

    public AccountManagerTestRule(@NonNull FakeAccountManagerFacade fakeAccountManagerFacade) {
        this(fakeAccountManagerFacade, new FakeAccountInfoService());
    }

    public AccountManagerTestRule(
            @NonNull FakeAccountManagerFacade fakeAccountManagerFacade,
            @Nullable FakeAccountInfoService fakeAccountInfoService) {
        mFakeAccountManagerFacade = fakeAccountManagerFacade;
        mFakeAccountInfoService = fakeAccountInfoService;
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUpRule();
                try {
                    statement.evaluate();
                } finally {
                    tearDownRule();
                }
            }
        };
    }

    /** Sets up the AccountManagerFacade mock. */
    private void setUpRule() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mFakeAccountInfoService != null) {
                        AccountInfoServiceProvider.setInstanceForTests(mFakeAccountInfoService);
                    }
                });
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    /** Tears down the AccountManagerFacade mock and signs out if user is signed in. */
    private void tearDownRule() {
        if (mFakeAccountInfoService != null) AccountInfoServiceProvider.resetForTests();
    }

    /**
     * Adds an observer that detects changes in the account state propagated by the IdentityManager
     * object.
     */
    public void observeIdentityManager(IdentityManager identityManager) {
        identityManager.addObserver(mFakeAccountInfoService);
    }

    /**
     * Adds an account to the fake AccountManagerFacade and {@link AccountInfo} to {@link
     * FakeAccountInfoService}.
     */
    public void addAccount(AccountInfo accountInfo) {
        mFakeAccountManagerFacade.addAccount(accountInfo);
        // TODO(crbug.com/40234741): Revise this test rule and remove the condition here.
        if (mFakeAccountInfoService != null) mFakeAccountInfoService.addAccountInfo(accountInfo);
    }

    /** Updates an account in the fake AccountManagerFacade and {@link FakeAccountInfoService}. */
    public void updateAccount(AccountInfo accountInfo) {
        mFakeAccountManagerFacade.updateAccount(accountInfo);
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
    }

    public void setAccountFetchFailed() {
        mFakeAccountManagerFacade.setAccountFetchFailed();
    }

    /** See {@link FakeAccountManagerFacade#blockGetAccounts(boolean)}. */
    public FakeAccountManagerFacade.UpdateBlocker blockGetAccountsUpdate(boolean populateCache) {
        return mFakeAccountManagerFacade.blockGetAccounts(populateCache);
    }
}
