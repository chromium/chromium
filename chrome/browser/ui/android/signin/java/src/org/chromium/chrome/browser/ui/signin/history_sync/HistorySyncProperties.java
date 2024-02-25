// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.view.View;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class HistorySyncProperties {
    static final PropertyModel.WritableObjectPropertyKey<DisplayableProfileData> PROFILE_DATA =
            new PropertyModel.WritableObjectPropertyKey<>("profile_data");

    // PropertyKey for the "accept" button
    static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener> ON_ACCEPT_CLICKED =
            new PropertyModel.ReadableObjectPropertyKey<>("on_accept_clicked");

    // PropertyKey for the "decline" button
    static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener> ON_DECLINE_CLICKED =
            new PropertyModel.ReadableObjectPropertyKey<>("on_decline_clicked");

    // PropertyKey for the "more" button
    static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener> ON_MORE_CLICKED =
            new PropertyModel.ReadableObjectPropertyKey<>("on_more_clicked");

    // PropertyKey for the "settings" link
    static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener> ON_SETTINGS_CLICKED =
            new PropertyModel.ReadableObjectPropertyKey<>("on_settings_clicked");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PROFILE_DATA,
                ON_ACCEPT_CLICKED,
                ON_DECLINE_CLICKED,
                ON_MORE_CLICKED,
                ON_SETTINGS_CLICKED
            };

    private HistorySyncProperties() {}

    static PropertyModel createModel(
            DisplayableProfileData profileData,
            Runnable onAcceptClicked,
            Runnable onDeclineClicked,
            Runnable onMoreClicked,
            Runnable onSettingsClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(PROFILE_DATA, profileData)
                .with(ON_ACCEPT_CLICKED, v -> onAcceptClicked.run())
                .with(ON_DECLINE_CLICKED, v -> onDeclineClicked.run())
                .with(ON_MORE_CLICKED, v -> onMoreClicked.run())
                .with(ON_SETTINGS_CLICKED, v -> onSettingsClicked.run())
                .build();
    }
}
