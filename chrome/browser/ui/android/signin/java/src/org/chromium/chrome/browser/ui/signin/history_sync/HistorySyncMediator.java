// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;

import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.ui.modelutil.PropertyModel;

class HistorySyncMediator {
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;

    HistorySyncMediator(Context context) {
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mModel =
                HistorySyncProperties.createModel(
                        mProfileDataCache.getProfileDataOrDefault(""),
                        this::onAcceptClicked,
                        this::onDeclineClicked,
                        this::onMoreClicked,
                        this::onSettingsClicked);
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onAcceptClicked() {
        // TODO(crbug.com/1520791): Implement this method
    }

    private void onDeclineClicked() {
        // TODO(crbug.com/1520791): Implement this method
    }

    private void onMoreClicked() {
        // TODO(crbug.com/1520791): Implement this method
    }

    private void onSettingsClicked() {
        // TODO(crbug.com/1520791): Implement this method
    }
}
