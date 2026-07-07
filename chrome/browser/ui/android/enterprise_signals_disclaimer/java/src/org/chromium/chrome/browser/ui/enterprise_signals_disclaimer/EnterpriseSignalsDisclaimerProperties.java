// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties defined for the enterprise signals disclaimer. */
@NullMarked
class EnterpriseSignalsDisclaimerProperties {
    public static final WritableObjectPropertyKey<String> TITLE =
            new WritableObjectPropertyKey<>("title");
    public static final WritableObjectPropertyKey<String> DESCRIPTION =
            new WritableObjectPropertyKey<>("description");
    public static final WritableObjectPropertyKey<String> PROFILE_INFORMATION_TITLE =
            new WritableObjectPropertyKey<>("profile_information_title");
    public static final WritableObjectPropertyKey<String> PROFILE_INFORMATION_DETAILS =
            new WritableObjectPropertyKey<>("profile_information_details");
    public static final WritableObjectPropertyKey<String> DEVICE_INFORMATION_TITLE =
            new WritableObjectPropertyKey<>("device_information_title");
    public static final WritableObjectPropertyKey<String> DEVICE_INFORMATION_DETAILS =
            new WritableObjectPropertyKey<>("device_information_details");
    public static final WritableObjectPropertyKey<String> ACCEPT_BUTTON_TEXT =
            new WritableObjectPropertyKey<>("accept_button_text");
    public static final WritableObjectPropertyKey<String> CANCEL_BUTTON_TEXT =
            new WritableObjectPropertyKey<>("cancel_button_text");
    public static final WritableObjectPropertyKey<OnClickListener> ON_ACCEPT_CLICKED =
            new WritableObjectPropertyKey<>("on_accept_clicked");
    public static final WritableObjectPropertyKey<OnClickListener> ON_CANCEL_CLICKED =
            new WritableObjectPropertyKey<>("on_cancel_clicked");

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TITLE,
                DESCRIPTION,
                PROFILE_INFORMATION_TITLE,
                PROFILE_INFORMATION_DETAILS,
                DEVICE_INFORMATION_TITLE,
                DEVICE_INFORMATION_DETAILS,
                ACCEPT_BUTTON_TEXT,
                CANCEL_BUTTON_TEXT,
                ON_ACCEPT_CLICKED,
                ON_CANCEL_CLICKED,
            };

    private EnterpriseSignalsDisclaimerProperties() {}
}
