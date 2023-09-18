// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;

import org.chromium.components.webapps.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The Mediator for the PWA Restore bottom sheet.
 */
class PwaRestoreBottomSheetMediator {
    // The current activity.
    private final Activity mActivity;

    // The underlying property model for the bottom sheeet.
    private final PropertyModel mModel;

    PwaRestoreBottomSheetMediator(Activity activity) {
        mActivity = activity;
        mModel = PwaRestoreProperties.createModel();

        setPeekingState();
    }

    private void setPeekingState() {
        mModel.set(PwaRestoreProperties.TITLE,
                mActivity.getString(R.string.pwa_restore_title_peeking));
        mModel.set(PwaRestoreProperties.DESCRIPTION,
                mActivity.getString(R.string.pwa_restore_description_peeking));
        mModel.set(PwaRestoreProperties.BUTTON_LABEL,
                mActivity.getString(R.string.pwa_restore_button_peeking));
        mModel.set(PwaRestoreProperties.CAN_SUBMIT, true);
    }

    PropertyModel getModel() {
        return mModel;
    }
}
