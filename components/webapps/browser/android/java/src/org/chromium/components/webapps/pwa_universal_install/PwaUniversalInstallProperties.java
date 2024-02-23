// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Contains the properties that a pwa-universal-install {@link PropertyModel} can have. */
public class PwaUniversalInstallProperties {
    /** View states of the PWA Restore Bottom Sheet. */
    @IntDef({
        ViewState.CHECKING_APP,
        ViewState.APP_ALREADY_INSTALLED,
        ViewState.APP_IS_INSTALLABLE,
        ViewState.APP_IS_NOT_INSTALLABLE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ViewState {
        int CHECKING_APP = 0;
        int APP_ALREADY_INSTALLED = 1;
        int APP_IS_INSTALLABLE = 2;
        int APP_IS_NOT_INSTALLABLE = 3;
    }

    // PropertyKey indicating the view state of the bottom sheet:
    static final WritableIntPropertyKey VIEW_STATE = new WritableIntPropertyKey();

    // Simple labels and states:
    static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    // Button handling:
    static final ReadableObjectPropertyKey<OnClickListener> INSTALL_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> OPEN_APP_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        VIEW_STATE,
        TITLE,
        INSTALL_BUTTON_ON_CLICK_CALLBACK,
        ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK,
        OPEN_APP_BUTTON_ON_CLICK_CALLBACK
    };

    static PropertyModel createModel(
            Runnable onInstallClicked, Runnable onAddShortcutClicked, Runnable onOpenAppClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(INSTALL_BUTTON_ON_CLICK_CALLBACK, v -> onInstallClicked.run())
                .with(ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK, v -> onAddShortcutClicked.run())
                .with(OPEN_APP_BUTTON_ON_CLICK_CALLBACK, v -> onOpenAppClicked.run())
                .build();
    }
}
