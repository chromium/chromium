// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

/**
 * Lists base::Features that can be accessed through {@link ModalDialogFeatureMap}.
 *
 * <p>Should be kept in sync with |kFeaturesExposedToJava| in
 * //components/browser_ui/modaldialog/android/modaldialog_feature_map.cc
 */
public abstract class ModalDialogFeatureList {
    public static final String MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS =
            "ModalDialogLayoutWithSystemInsets";
}
