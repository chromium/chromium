// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ContextUtils;
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

    /* Use ACCOUNT2 when signing in or adding a default adult user to the device as a secondary user.*/
    public static final AccountInfo ACCOUNT2 =
            new AccountInfo.Builder(
                            "test2@gmail.com", FakeAccountManagerFacade.toGaiaId("test2@gmail.com"))
                    .fullName("Test2 Full")
                    .givenName("Test2 Given")
                    .accountImage(createAvatar())
                    .build();

    /* Use CHILD_ACCOUNT when you need a supervised user signed in */
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
