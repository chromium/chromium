// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;

import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Creates the AccountSelection component.
 */
public class AccountSelectionCoordinator implements AccountSelectionComponent {
    private final AccountSelectionMediator mMediator = new AccountSelectionMediator();
    private final PropertyModel mModel =
            AccountSelectionProperties.createDefaultModel(mMediator::onDismissed);

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            AccountSelectionComponent.Delegate delegate) {
        mMediator.initialize(delegate, mModel);
        // TODO(majidvp): Create the view and setup model change processor.
    }

    @Override
    public void showAccounts(String url, List<Account> accounts) {
        mMediator.showAccounts(url, accounts);
    }
}
