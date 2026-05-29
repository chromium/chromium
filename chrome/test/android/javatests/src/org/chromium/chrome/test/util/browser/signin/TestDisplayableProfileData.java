// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.graphics.drawable.BitmapDrawable;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * This class provides standard test instances of {@link DisplayableProfileData} to be used in
 * signin-related tests.
 */
// TODO(crbug.com/517542462): move this inside TestAccounts
public class TestDisplayableProfileData {

    public static final DisplayableProfileData ACCOUNT1 = profileDataOf(TestAccounts.ACCOUNT1);
    public static final DisplayableProfileData ACCOUNT2 = profileDataOf(TestAccounts.ACCOUNT2);

    /**
     * Converts an {@link AccountInfo} to a {@link DisplayableProfileData}.
     *
     * @param accountInfo The account info to convert.
     * @return The displayable profile data or null if accountInfo is null.
     */
    public static @Nullable DisplayableProfileData profileDataOf(
            @Nullable AccountInfo accountInfo) {
        if (accountInfo == null) {
            return null;
        }
        return new DisplayableProfileData(
                accountInfo.getId(),
                accountInfo.getEmail(),
                new BitmapDrawable(accountInfo.getAccountImage()),
                accountInfo.getFullName(),
                accountInfo.getGivenName(),
                true);
    }

    private TestDisplayableProfileData() {}
}
