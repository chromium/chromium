// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.hamcrest.Matcher;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * This test rule mocks AccountManagerFacade.
 *
 * <p>The rule will not invoke any native code, therefore it is safe to use it in Robolectric tests.
 */
public class AccountManagerTestRule implements TestRule {
    // The matcher for the add account button in the fake add account activity.
    public static final Matcher<View> ADD_ACCOUNT_BUTTON_MATCHER =
            withId(FakeAccountManagerFacade.AddAccountActivityStub.OK_BUTTON_ID);
    // The matcher for the cancel button in the fake add account activity.
    public static final Matcher<View> CANCEL_ADD_ACCOUNT_BUTTON_MATCHER =
            withId(FakeAccountManagerFacade.AddAccountActivityStub.CANCEL_BUTTON_ID);

    // TODO(crbug.com/40890215): Use TEST_ACCOUNT_1 instead.
    @Deprecated public static final String TEST_ACCOUNT_EMAIL = "test@gmail.com";

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
    public void setUpRule() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mFakeAccountInfoService != null) {
                        AccountInfoServiceProvider.setInstanceForTests(mFakeAccountInfoService);
                    }
                });
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    /** Tears down the AccountManagerFacade mock and signs out if user is signed in. */
    public void tearDownRule() {
        if (mFakeAccountInfoService != null) AccountInfoServiceProvider.resetForTests();
    }

    /**
     * Adds an observer that detects changes in the account state propagated by the IdentityManager
     * object.
     */
    public void observeIdentityManager(IdentityManager identityManager) {
        identityManager.addObserver(mFakeAccountInfoService);
    }

    // TODO(crbug.com/40890215): Remove deprecated `addAccount` overloads.
    /**
     * Adds an account of the given accountName to the fake AccountManagerFacade.
     *
     * @return The CoreAccountInfo for the account added.
     */
    @Deprecated
    public AccountInfo addAccount(String accountName) {
        final String baseName = accountName.split("@", 2)[0];
        AccountInfo accountInfo =
                new AccountInfo.Builder(accountName, FakeAccountManagerFacade.toGaiaId(accountName))
                        .fullName(baseName + ".full")
                        .givenName(baseName + ".given")
                        .accountImage(createAvatar())
                        .build();
        addAccount(accountInfo);
        return accountInfo;
    }

    /**
     * Adds an account to the fake AccountManagerFacade and {@link AccountInfo} to {@link
     * FakeAccountInfoService}.
     */
    @Deprecated
    public AccountInfo addAccount(
            String email, String fullName, String givenName, @Nullable Bitmap avatar) {
        AccountInfo accountInfo =
                new AccountInfo.Builder(email, FakeAccountManagerFacade.toGaiaId(email))
                        .fullName(fullName)
                        .givenName(givenName)
                        .accountImage(avatar)
                        .build();
        addAccount(accountInfo);
        return accountInfo;
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

    /** See {@link FakeAccountManagerFacade#blockGetCoreAccountInfos(boolean)}. */
    public FakeAccountManagerFacade.UpdateBlocker blockGetCoreAccountInfosUpdate(
            boolean populateCache) {
        return mFakeAccountManagerFacade.blockGetCoreAccountInfos(populateCache);
    }

    /**
     * Returns an avatar image created from test resource.
     *
     * <p>TODO(crbug.com/40890215): Remove this after deleting the deprecated `addAccount` overload
     * which calls it.
     */
    private static Bitmap createAvatar() {
        Drawable drawable =
                AppCompatResources.getDrawable(
                        ContextUtils.getApplicationContext(), R.drawable.test_profile_picture);
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    /**
     * Resolves the minor mode of {@param accountInfo} to restricted, so that the UI will be safe to
     * show to minors.
     */
    public void resolveMinorModeToRestricted(CoreAccountId accountId) {
        // TODO(b/343384614): append instead of overriding
        overrideCapabilities(accountId, TestAccounts.MINOR_MODE_REQUIRED);
    }

    private void overrideCapabilities(CoreAccountId accountId, AccountCapabilities capabilities) {
        mFakeAccountManagerFacade.setAccountCapabilities(accountId, capabilities);
    }
}
