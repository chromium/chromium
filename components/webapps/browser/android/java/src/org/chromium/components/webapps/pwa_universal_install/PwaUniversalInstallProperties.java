// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Contains the properties that a pwa-universal-install {@link PropertyModel} can have. */
public class PwaUniversalInstallProperties {
    // Simple labels:
    static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    // Button handling:
    static final ReadableObjectPropertyKey<OnClickListener> INSTALL_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        TITLE, INSTALL_BUTTON_ON_CLICK_CALLBACK, ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK
    };

    static PropertyModel createModel(Runnable onInstallClicked, Runnable onAddShortcutClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(INSTALL_BUTTON_ON_CLICK_CALLBACK, v -> onInstallClicked.run())
                .with(ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK, v -> onAddShortcutClicked.run())
                .build();
    }
}
