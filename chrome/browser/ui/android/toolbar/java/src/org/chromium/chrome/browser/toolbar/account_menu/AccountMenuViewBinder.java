// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the Account Menu popup. */
@NullMarked
public class AccountMenuViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == AccountMenuProperties.AUTOFILL_CLICK_LISTENER) {
            View autofillRow = view.findViewById(R.id.account_menu_passwords_and_autofill);
            autofillRow.setOnClickListener(
                    model.get(AccountMenuProperties.AUTOFILL_CLICK_LISTENER));
        }
    }
}
