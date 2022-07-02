// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.HashMap;

/**
 * This test rule mocks AccountManagerFacade.
 *
 * TODO(crbug.com/1334286): Migrate usages that need native to {@link SigninTestRule} and remove
 * the methods that call native from this rule.
 *
 * The rule will not invoke any native code, therefore it is safe to use it in Robolectric tests.
 */
public class AccountManagerTestRule implements TestRule {
    public static final String TEST_ACCOUNT_EMAIL = "test@gmail.com";

    private final @NonNull FakeAccountManagerFacade mFakeAccountManagerFacade;
    private final @Nullable FakeAccountInfoService mFakeAccountInfoService;

    public AccountManagerTestRule() {
        this(new FakeAccountManagerFacade(), new FakeAccountInfoService());
    }

    public AccountManagerTestRule(@NonNull FakeAccountManagerFacade fakeAccountManagerFacade) {
        this(fakeAccountManagerFacade, new FakeAccountInfoService());
    }

    public AccountManagerTestRule(@NonNull FakeAccountManagerFacade fakeAccountManagerFacade,
            @NonNull FakeAccountInfoService fakeAccountInfoService) {
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

    /**
     * Sets up the AccountManagerFacade mock.
     */
    public void setUpRule() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { AccountInfoServiceProvider.setInstanceForTests(mFakeAccountInfoService); });
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    /**
     * Tears down the AccountManagerFacade mock and signs out if user is signed in.
     */
    public void tearDownRule() {
        AccountManagerFacadeProvider.resetInstanceForTests();
        AccountInfoServiceProvider.resetForTests();
    }

    /**
     * Adds an observer that detects changes in the account state propagated by the
     * IdentityManager object.
     */
    public void observeIdentityManager(IdentityManager identityManager) {
        identityManager.addObserver(mFakeAccountInfoService);
    }

    /**
     * Adds an account of the given accountName to the fake AccountManagerFacade.
     * @return The CoreAccountInfo for the account added.
     */
    public AccountInfo addAccount(String accountName) {
        return addAccount(accountName, new AccountCapabilities(new HashMap<>()));
    }

    /**
     * Adds an account of the given accountName and capabilities to the fake AccountManagerFacade.
     * @return The CoreAccountInfo for the account added.
     */
    public AccountInfo addAccount(String accountName, @NonNull AccountCapabilities capabilities) {
        assert mFakeAccountInfoService != null;
        final String baseName = accountName.split("@", 2)[0];
        return addAccount(
                accountName, baseName + ".full", baseName + ".given", createAvatar(), capabilities);
    }

    /**
     * Adds an account to the fake AccountManagerFacade and {@link AccountInfo} to
     * {@link FakeAccountInfoService}.
     */
    public AccountInfo addAccount(
            String email, String fullName, String givenName, @Nullable Bitmap avatar) {
        return addAccount(
                email, fullName, givenName, avatar, new AccountCapabilities(new HashMap<>()));
    }

    /**
     * Adds an account to the fake AccountManagerFacade and {@link AccountInfo} to
     * {@link FakeAccountInfoService}.
     */
    public AccountInfo addAccount(String email, String fullName, String givenName,
            @Nullable Bitmap avatar, @NonNull AccountCapabilities capabilities) {
        assert mFakeAccountInfoService != null;
        mFakeAccountManagerFacade.addAccount(AccountUtils.createAccountFromName(email));
        return mFakeAccountInfoService.addAccountInfo(
                email, fullName, givenName, avatar, capabilities);
    }

    /**
     * Removes an account with the given account email.
     */
    public void removeAccount(String accountEmail) {
        mFakeAccountManagerFacade.removeAccount(AccountUtils.createAccountFromName(accountEmail));
    }

    /**
     * Converts an account email to its corresponding CoreAccountInfo object.
     */
    public CoreAccountInfo toCoreAccountInfo(String accountEmail) {
        String accountGaiaId = mFakeAccountManagerFacade.getAccountGaiaId(accountEmail);
        return CoreAccountInfo.createFromEmailAndGaiaId(accountEmail, accountGaiaId);
    }

    /**
     * Creates an email used to identify child accounts in tests.
     * A child-specific prefix will be appended to the base name so that the created account
     * will be considered as {@link ChildAccountStatus#REGULAR_CHILD} in
     * {@link FakeAccountManagerFacade}.
     */
    public static String generateChildEmail(String baseName) {
        return FakeAccountManagerFacade.generateChildEmail(baseName);
    }

    /**
     * Returns an avatar image created from test resource.
     */
    private static Bitmap createAvatar() {
        Drawable drawable = AppCompatResources.getDrawable(
                ContextUtils.getApplicationContext(), R.drawable.test_profile_picture);
        Bitmap bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }
}
