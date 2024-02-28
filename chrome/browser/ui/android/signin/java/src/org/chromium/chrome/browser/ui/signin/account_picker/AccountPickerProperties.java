// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties for account picker. */
class AccountPickerProperties {
    private AccountPickerProperties() {}

    /** Properties for "add account" row in account picker. */
    static class AddAccountRowProperties {
        static final PropertyModel.ReadableObjectPropertyKey<OnClickListener> ON_CLICK_LISTENER =
                new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ON_CLICK_LISTENER};

        private AddAccountRowProperties() {}

        static PropertyModel createModel(Runnable runnableAddAccount) {
            return new PropertyModel.Builder(ALL_KEYS)
                    .with(ON_CLICK_LISTENER, v -> runnableAddAccount.run())
                    .build();
        }
    }

    /** Properties for account row in account picker. */
    static class ExistingAccountRowProperties {
        static final PropertyModel.WritableObjectPropertyKey<DisplayableProfileData> PROFILE_DATA =
                new PropertyModel.WritableObjectPropertyKey<>("profile_data");
        static final PropertyModel.WritableBooleanPropertyKey IS_CURRENTLY_SELECTED =
                new PropertyModel.WritableBooleanPropertyKey("is_currently_selected");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<DisplayableProfileData>>
                ON_CLICK_LISTENER =
                        new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS =
                new PropertyKey[] {PROFILE_DATA, IS_CURRENTLY_SELECTED, ON_CLICK_LISTENER};

        private ExistingAccountRowProperties() {}

        static PropertyModel createModel(
                DisplayableProfileData profileData,
                boolean isCurrentlySelected,
                Callback<DisplayableProfileData> listener) {
            return new PropertyModel.Builder(ALL_KEYS)
                    .with(PROFILE_DATA, profileData)
                    .with(IS_CURRENTLY_SELECTED, isCurrentlySelected)
                    .with(ON_CLICK_LISTENER, listener)
                    .build();
        }
    }

    /** Item types of account picker. */
    @IntDef({ItemType.EXISTING_ACCOUNT_ROW, ItemType.ADD_ACCOUNT_ROW})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /**
         * Item type for models created with {@link ExistingAccountRowProperties#createModel} and
         * use {@link ExistingAccountRowViewBinder} for view setup.
         */
        int EXISTING_ACCOUNT_ROW = 1;

        /**
         * Item type for models created with {@link AddAccountRowProperties#createModel} and
         * use {@link OnClickListenerViewBinder} for view setup.
         */
        int ADD_ACCOUNT_ROW = 2;
    }
}
