// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ContextUtils;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;

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
                    .accountImage(createAvatar())
                    .build();

    /*
     * Use ACCOUNT2 when signing in or adding a default adult user to the device as a secondary
     * user.
     */
    public static final AccountInfo ACCOUNT2 =
            new AccountInfo.Builder(
                            "test2@gmail.com", FakeAccountManagerFacade.toGaiaId("test2@gmail.com"))
                    .fullName("Test2 Full")
                    .givenName("Test2 Given")
                    .accountImage(createAvatar())
                    .build();

    /* Use CHILD_ACCOUNT when you need a supervised user signed in. */
    public static final AccountInfo CHILD_ACCOUNT =
            new AccountInfo.Builder(
                            FakeAccountManagerFacade.generateChildEmail(ACCOUNT1.getEmail()),
                            FakeAccountManagerFacade.toGaiaId(
                                    FakeAccountManagerFacade.generateChildEmail(
                                            ACCOUNT1.getEmail())))
                    .fullName("Test1 Full")
                    .givenName("Test1 Given")
                    .accountImage(createAvatar())
                    .accountCapabilities(
                            new AccountCapabilitiesBuilder()
                                    .setIsSubjectToParentalControls(true)
                                    .setCanShowHistorySyncOptInsWithoutMinorModeRestrictions(false)
                                    .build())
                    .build();

    public static final AccountInfo TEST_ACCOUNT_NO_NAME =
            new AccountInfo.Builder(
                            "test.noname@gmail.com",
                            FakeAccountManagerFacade.toGaiaId("test.noname@gmail.com"))
                    .fullName(null)
                    .givenName(null)
                    .build();

    /**
     * To be used in test cases where we want to test Signin flows for accounts that don't have a
     * displayable email.
     */
    public static final AccountInfo CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL =
            new AccountInfo.Builder(
                            generateChildEmail("test@gmail.com"),
                            FakeAccountManagerFacade.toGaiaId("test@gmail.com"))
                    .fullName("Test1 Full")
                    .givenName("Test1 Given")
                    .accountImage(createAvatar())
                    .accountCapabilities(
                            new AccountCapabilitiesBuilder()
                                    .setCanHaveEmailAddressDisplayed(false)
                                    .setIsSubjectToParentalControls(true)
                                    .build())
                    .build();

    /**
     * To be used in test cases where we want to test Signin flows for accounts that don't have a
     * displayable name or email.
     */
    public static final AccountInfo TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME =
            new AccountInfo.Builder(
                            generateChildEmail("test@gmail.com"),
                            FakeAccountManagerFacade.toGaiaId("test@gmail.com"))
                    .accountImage(createAvatar())
                    .accountCapabilities(
                            new AccountCapabilitiesBuilder()
                                    .setCanHaveEmailAddressDisplayed(false)
                                    .setIsSubjectToParentalControls(true)
                                    .build())
                    .build();

    private static final AccountCapabilities MINOR_MODE_NOT_REQUIRED =
            new AccountCapabilitiesBuilder()
                    .setCanShowHistorySyncOptInsWithoutMinorModeRestrictions(true)
                    .build();

    /** To be used in tests which need an account without AADC minor restrictions. */
    public static final AccountInfo AADC_ADULT_ACCOUNT =
            new AccountInfo.Builder(
                            "aadc.adult.account@gmail.com",
                            FakeAccountManagerFacade.toGaiaId("aadc.adult.account@gmail.com"))
                    .fullName("AADC Adult")
                    .givenName("AADC Adult Account")
                    .accountImage(createAvatar())
                    .accountCapabilities(MINOR_MODE_NOT_REQUIRED)
                    .build();

    public static final AccountCapabilities MINOR_MODE_REQUIRED =
            new AccountCapabilitiesBuilder()
                    .setCanShowHistorySyncOptInsWithoutMinorModeRestrictions(false)
                    .build();

    /** To be used in tests which need an account with AADC minor restrictions. */
    public static final AccountInfo AADC_MINOR_ACCOUNT =
            new AccountInfo.Builder(
                            "aadc.minor.account@gmail.com",
                            FakeAccountManagerFacade.toGaiaId("aadc.minor.account@gmail.com"))
                    .fullName("AADC Minor")
                    .givenName("AADC Minor Account")
                    .accountImage(createAvatar())
                    .accountCapabilities(MINOR_MODE_REQUIRED)
                    .build();

    /**
     * To be used in tests which explicitly need to test behavior before AADC restrictions have been
     * determined
     */
    public static final AccountInfo AADC_UNRESOLVED_ACCOUNT = ACCOUNT1;

    /* An enterprise managed account with a hosted domain specified. */
    public static final AccountInfo MANAGED_ACCOUNT =
            new AccountInfo.Builder(
                            "test@example.com",
                            FakeAccountManagerFacade.toGaiaId("test@example.com"))
                    .fullName("Managed Full")
                    .givenName("Managed Given")
                    .hostedDomain("example.com")
                    .accountImage(createAvatar())
                    .build();

    /**
     * Creates an email used to identify child accounts in tests. A child-specific prefix will be
     * appended to the base name so that the created account will be considered as {@link
     * ChildAccountStatus#REGULAR_CHILD} in {@link FakeAccountManagerFacade}.
     */
    private static String generateChildEmail(String baseName) {
        return FakeAccountManagerFacade.generateChildEmail(baseName);
    }

    /** Returns an avatar image created from test resource. */
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
}
