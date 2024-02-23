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

    PwaUniversalInstallBottomSheetMediator(
            Activity activity,
            boolean webAppAlreadyInstalled,
            Runnable installCallback,
            Runnable addShortcutCallback,
            Runnable openAppCallback) {
        mActivity = activity;
        mModel =
                PwaUniversalInstallProperties.createModel(
                        installCallback, addShortcutCallback, openAppCallback);
        mModel.set(
                PwaUniversalInstallProperties.TITLE,
                mActivity.getString(R.string.pwa_uni_install_title));
        mModel.set(
                PwaUniversalInstallProperties.VIEW_STATE,
                webAppAlreadyInstalled
                        ? PwaUniversalInstallProperties.ViewState.APP_ALREADY_INSTALLED
                        : PwaUniversalInstallProperties.ViewState.CHECKING_APP);
    }

    PropertyModel getModel() {
        return mModel;
    }
}
