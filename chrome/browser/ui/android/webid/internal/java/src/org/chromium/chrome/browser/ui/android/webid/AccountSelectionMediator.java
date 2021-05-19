// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    private AccountSelectionComponent.Delegate mDelegate;
    private PropertyModel mModel;

    void initialize(AccountSelectionComponent.Delegate delegate, PropertyModel model) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
    }

    void showAccounts(String url, List<Account> accounts) {}
}
