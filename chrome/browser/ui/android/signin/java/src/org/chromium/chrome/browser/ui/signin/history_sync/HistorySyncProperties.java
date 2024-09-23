// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper.ScreenMode;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class HistorySyncProperties {
    static final PropertyModel.WritableObjectPropertyKey<DisplayableProfileData> PROFILE_DATA =
            new PropertyModel.WritableObjectPropertyKey<>("profile_data");

    // PropertyKey for the "accept" button
    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener> ON_ACCEPT_CLICKED =
            new PropertyModel.WritableObjectPropertyKey<>("on_accept_clicked");

    // PropertyKey for the "decline" button
    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener> ON_DECLINE_CLICKED =
            new PropertyModel.WritableObjectPropertyKey<>("on_decline_clicked");

    static final PropertyModel.WritableObjectPropertyKey<CharSequence> FOOTER_STRING =
            new PropertyModel.WritableObjectPropertyKey<>("footer_string");

    static final PropertyModel.WritableIntPropertyKey MINOR_MODE_RESTRICTION_STATUS =
            new PropertyModel.WritableIntPropertyKey("minor_mode_restriction_status");

    static final PropertyModel.WritableBooleanPropertyKey USE_LANDSCAPE_LAYOUT =
            new PropertyModel.WritableBooleanPropertyKey("use_landscape_layout");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PROFILE_DATA,
                ON_ACCEPT_CLICKED,
                ON_DECLINE_CLICKED,
                FOOTER_STRING,
                MINOR_MODE_RESTRICTION_STATUS,
                USE_LANDSCAPE_LAYOUT
            };

    private HistorySyncProperties() {}

    static PropertyModel createModel(
            DisplayableProfileData profileData,
            OnClickListener onAcceptClicked,
            OnClickListener onDeclineClicked,
            String footerString,
            Boolean useLandscapeLayout) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(PROFILE_DATA, profileData)
                .with(ON_ACCEPT_CLICKED, onAcceptClicked)
                .with(ON_DECLINE_CLICKED, onDeclineClicked)
                .with(FOOTER_STRING, footerString)
                .with(USE_LANDSCAPE_LAYOUT, useLandscapeLayout)
                .with(MINOR_MODE_RESTRICTION_STATUS, ScreenMode.PENDING)
                .build();
    }
}
