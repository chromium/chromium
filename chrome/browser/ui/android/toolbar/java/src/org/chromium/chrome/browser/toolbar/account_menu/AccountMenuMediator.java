// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator managing business logic and menu items for the Account Menu popup. */
@NullMarked
public class AccountMenuMediator {
    private final Context mContext;
    private final PropertyModel mModel;
    private final Runnable mDismissCallback;

    public AccountMenuMediator(Context context, PropertyModel model, Runnable dismissCallback) {
        mContext = context;
        mModel = model;
        mDismissCallback = dismissCallback;

        updateMenuItems();
    }

    /** Populates the menu action items. */
    public void updateMenuItems() {
        mModel.set(
                AccountMenuProperties.AUTOFILL_CLICK_LISTENER,
                v -> {
                    mDismissCallback.run();
                    openAutofillSettings();
                });
    }

    private void openAutofillSettings() {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, SettingsFragment.AUTOFILL_AND_PASSWORDS);
    }
}
