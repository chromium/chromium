// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertThrows;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig.OptInMode;
import org.chromium.components.signin.test.util.TestAccounts;

/** Unit tests for {@link BottomSheetSigninAndHistorySyncConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetSigninAndHistorySyncConfigTest {

    private static final AccountPickerBottomSheetStrings BOTTOM_SHEET_STRINGS =
            new AccountPickerBottomSheetStrings.Builder("Bottom Sheet Title").build();
    private static final String HISTORY_SYNC_TITLE = "History Sync Title";
    private static final String HISTORY_SYNC_SUBTITLE = "History Sync Subtitle";

    @Test
    @SmallTest
    public void testSeamlessSigninWithMissingAccountId_throwsAssertion() {
        assertThrows(
                AssertionError.class,
                () -> {
                    new BottomSheetSigninAndHistorySyncConfig.Builder(
                                    BOTTOM_SHEET_STRINGS,
                                    NoAccountSigninMode.BOTTOM_SHEET,
                                    WithAccountSigninMode.SEAMLESS_SIGNIN,
                                    OptInMode.OPTIONAL,
                                    HISTORY_SYNC_TITLE,
                                    HISTORY_SYNC_SUBTITLE)
                            .shouldShowSigninSnackbar(true)
                            .build();
                });
    }

    @Test
    @SmallTest
    public void testSeamlessSigninWithoutSnackbarEnabled_throwsAssertion() {
        assertThrows(
                AssertionError.class,
                () -> {
                    new BottomSheetSigninAndHistorySyncConfig.Builder(
                                    BOTTOM_SHEET_STRINGS,
                                    NoAccountSigninMode.BOTTOM_SHEET,
                                    WithAccountSigninMode.SEAMLESS_SIGNIN,
                                    OptInMode.OPTIONAL,
                                    HISTORY_SYNC_TITLE,
                                    HISTORY_SYNC_SUBTITLE)
                            .selectedCoreAccountId(TestAccounts.ACCOUNT1.getId())
                            .build();
                });
    }
}
