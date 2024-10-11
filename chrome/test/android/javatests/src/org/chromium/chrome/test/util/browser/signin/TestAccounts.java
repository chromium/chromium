// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * This class provides tests accounts to be used for signing in tests.
 *
 * <p>This class is used only in tests.
 */
public class TestAccounts {

    // TODO(crbug.com/40234741): Migrate tests that don't need to create their own accounts to these
    // constants.

    /* Use ACCOUNT1 when signing in or adding a default adult user to the device. */
    public static final AccountInfo ACCOUNT1 =
            new AccountInfo.Builder(
                            "test@gmail.com", FakeAccountManagerFacade.toGaiaId("test@gmail.com"))
                    .fullName("Test1 Full")
                    .givenName("Test1 Given")
                    .accountImage(AccountManagerTestRule.createAvatar())
                    .build();

    /* Use ACCOUNT2 when signing in or adding a default adult user to the device as a secondary user.*/
    public static final AccountInfo ACCOUNT2 =
            new AccountInfo.Builder(
                            "test2@gmail.com", FakeAccountManagerFacade.toGaiaId("test2@gmail.com"))
                    .fullName("Test2 Full")
                    .givenName("Test2 Given")
                    .accountImage(AccountManagerTestRule.createAvatar())
                    .build();

    /* Use CHILD_ACCOUNT when you need a supervised user signed in */
    public static final AccountInfo CHILD_ACCOUNT =
            new AccountInfo.Builder(
                            AccountManagerTestRule.generateChildEmail(ACCOUNT1.getEmail()),
                            FakeAccountManagerFacade.toGaiaId(
                                    AccountManagerTestRule.generateChildEmail(ACCOUNT1.getEmail())))
                    .fullName("Test1 Full")
                    .givenName("Test1 Given")
                    .accountImage(AccountManagerTestRule.createAvatar())
                    .accountCapabilities(
                            new AccountCapabilitiesBuilder()
                                    .setIsSubjectToParentalControls(true)
                                    .setCanShowHistorySyncOptInsWithoutMinorModeRestrictions(false)
                                    .build())
                    .build();
}
