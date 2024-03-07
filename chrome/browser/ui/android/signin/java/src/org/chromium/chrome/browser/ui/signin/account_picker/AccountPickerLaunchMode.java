// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Indicates which view should be shown first when the bottom sheet is displayed. */
@IntDef({
    AccountPickerLaunchMode.DEFAULT,
    AccountPickerLaunchMode.CHOOSE_ACCOUNT,
})
@Retention(RetentionPolicy.SOURCE)
public @interface AccountPickerLaunchMode {
    // The bottom sheet first shows the collapsed view with the default account or
    // the no-account view.
    int DEFAULT = 0;
    // The bottom sheet first shows the expanded view with the accounts list.
    int CHOOSE_ACCOUNT = 1;
}
