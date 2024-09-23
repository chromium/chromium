// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import org.chromium.chrome.browser.ui.signin.MinorModeHelper;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper.ScreenMode;
import org.chromium.components.signin.metrics.SyncButtonsType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class HistorySyncViewBinder {

    private static boolean sMinorModeButtonShownMetricRecorded;

    public static void bind(PropertyModel model, HistorySyncView view, PropertyKey key) {
        if (key == HistorySyncProperties.PROFILE_DATA) {
            view.getAccountImageView()
                    .setImageDrawable(model.get(HistorySyncProperties.PROFILE_DATA).getImage());
        } else if (key == HistorySyncProperties.FOOTER_STRING) {
            view.getDetailsDescription().setText(model.get(HistorySyncProperties.FOOTER_STRING));
        } else if (key == HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS
                || key == HistorySyncProperties.USE_LANDSCAPE_LAYOUT
                || key == HistorySyncProperties.ON_ACCEPT_CLICKED
                || key == HistorySyncProperties.ON_DECLINE_CLICKED) {
            view.maybeCreateButtons(
                    model.get(HistorySyncProperties.USE_LANDSCAPE_LAYOUT),
                    model.get(HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS));

            if (view.getAcceptButton() == null || view.getDeclineButton() == null) {
                assert model.get(HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS)
                        == ScreenMode.PENDING;
                return;
            }

            if (!sMinorModeButtonShownMetricRecorded) {
                switch (model.get(HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS)) {
                    case ScreenMode.RESTRICTED:
                        MinorModeHelper.recordButtonsShown(
                                SyncButtonsType.HISTORY_SYNC_EQUAL_WEIGHTED_FROM_CAPABILITY);
                        break;
                    case ScreenMode.UNRESTRICTED:
                        MinorModeHelper.recordButtonsShown(
                                SyncButtonsType.HISTORY_SYNC_NOT_EQUAL_WEIGHTED);
                        break;
                    case ScreenMode.DEADLINED:
                        MinorModeHelper.recordButtonsShown(
                                SyncButtonsType.HISTORY_SYNC_EQUAL_WEIGHTED_FROM_DEADLINE);
                        break;
                }
                sMinorModeButtonShownMetricRecorded = true;
            }

            view.getAcceptButton()
                    .setOnClickListener(model.get(HistorySyncProperties.ON_ACCEPT_CLICKED));
            view.getDeclineButton()
                    .setOnClickListener(model.get(HistorySyncProperties.ON_DECLINE_CLICKED));

        } else {
            throw new IllegalArgumentException("Unknown property key: " + key);
        }
    }
}
