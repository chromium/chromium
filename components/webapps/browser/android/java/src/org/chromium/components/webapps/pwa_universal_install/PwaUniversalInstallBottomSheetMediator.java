// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.app.Activity;

import org.chromium.components.webapps.R;
import org.chromium.ui.modelutil.PropertyModel;

/** The Mediator for the PWA Universal Install bottom sheet. */
class PwaUniversalInstallBottomSheetMediator {
    // The current activity.
    private final Activity mActivity;

    // The underlying property model for the bottom sheeet.
    private final PropertyModel mModel;

    PwaUniversalInstallBottomSheetMediator(Activity activity) {
        mActivity = activity;
        mModel = PwaUniversalInstallProperties.createModel();

        setPeekingState();
    }

    private void setPeekingState() {
        mModel.set(
                PwaUniversalInstallProperties.TITLE,
                mActivity.getString(R.string.pwa_uni_install_title));
    }

    PropertyModel getModel() {
        return mModel;
    }
}
