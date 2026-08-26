// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.build.annotations.NullMarked;

/** Presenter interface for displaying and managing AccountPicker UI presentation. */
@NullMarked
public interface AccountPickerPresenter {
    /** Shows the account picker UI. */
    void show(AccountPickerBottomSheetView view);

    /** Dismisses the account picker UI. */
    void dismiss();

    /** Destroys the presenter and cleans up registered observers. */
    void destroy();
}
