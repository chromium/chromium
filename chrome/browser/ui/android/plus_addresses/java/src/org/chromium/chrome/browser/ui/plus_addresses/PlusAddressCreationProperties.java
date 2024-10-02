// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Defines the data model for the {@code PlusAddressCreationView}. */
class PlusAddressCreationProperties {
    static final ReadableObjectPropertyKey<PlusAddressCreationNormalStateInfo> NORMAL_STATE_INFO =
            new ReadableObjectPropertyKey<>("normal_state_info");
    static final ReadableObjectPropertyKey<PlusAddressCreationDelegate> DELEGATE =
            new ReadableObjectPropertyKey<>("delegate");
    static final ReadableBooleanPropertyKey SHOW_ONBOARDING_NOTICE =
            new ReadableBooleanPropertyKey("show_onboarding_notice");
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableBooleanPropertyKey PLUS_ADDRESS_ICON_VISIBLE =
            new WritableBooleanPropertyKey("plus_address_icon_visible");
    static final WritableBooleanPropertyKey PLUS_ADDRESS_LOADING_VIEW_VISIBLE =
            new WritableBooleanPropertyKey("plus_address_loading_view_visible");
    static final WritableObjectPropertyKey<String> PROPOSED_PLUS_ADDRESS =
            new WritableObjectPropertyKey<>("proposed_plus_address");
    static final WritableBooleanPropertyKey REFRESH_ICON_ENABLED =
            new WritableBooleanPropertyKey("refresh_icon_enabled");
    static final WritableBooleanPropertyKey REFRESH_ICON_VISIBLE =
            new WritableBooleanPropertyKey("refresh_icon_visible");
    static final WritableBooleanPropertyKey CONFIRM_BUTTON_ENABLED =
            new WritableBooleanPropertyKey("confirm_button_enabled");
    static final WritableBooleanPropertyKey CONFIRM_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey("confirm_button_visible");
    static final WritableBooleanPropertyKey CANCEL_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey("cancel_button_visible");
    static final WritableBooleanPropertyKey LOADING_INDICATOR_VISIBLE =
            new WritableBooleanPropertyKey("loading_indicator_visible");

    // TODO: crbug.com/354881207 - Remove once enhanced error handling is launched.
    static final WritableBooleanPropertyKey LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE =
            new WritableBooleanPropertyKey("legacy_error_reporting_instruction_visible");

    static final WritableObjectPropertyKey<PlusAddressCreationErrorStateInfo> ERROR_STATE_INFO =
            new WritableObjectPropertyKey<>("error_state_info");

    static final PropertyKey[] ALL_KEYS = {
        NORMAL_STATE_INFO,
        DELEGATE,
        SHOW_ONBOARDING_NOTICE,
        VISIBLE,
        PLUS_ADDRESS_ICON_VISIBLE,
        PLUS_ADDRESS_LOADING_VIEW_VISIBLE,
        PROPOSED_PLUS_ADDRESS,
        REFRESH_ICON_ENABLED,
        REFRESH_ICON_VISIBLE,
        CONFIRM_BUTTON_ENABLED,
        CONFIRM_BUTTON_VISIBLE,
        CANCEL_BUTTON_VISIBLE,
        LOADING_INDICATOR_VISIBLE,
        LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE,
        ERROR_STATE_INFO,
    };

    private PlusAddressCreationProperties() {}
}
