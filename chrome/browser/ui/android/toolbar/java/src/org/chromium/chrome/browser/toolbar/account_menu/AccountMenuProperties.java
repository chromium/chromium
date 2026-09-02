// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Account Menu popup. */
@NullMarked
public class AccountMenuProperties {
    /** Click listener for the Passwords and autofill item. */
    public static final WritableObjectPropertyKey<OnClickListener> AUTOFILL_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {AUTOFILL_CLICK_LISTENER};
}
