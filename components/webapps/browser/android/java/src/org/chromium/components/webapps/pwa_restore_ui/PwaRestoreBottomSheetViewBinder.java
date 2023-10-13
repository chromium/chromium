// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.widget.Button;
import android.widget.TextView;

import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binds a pwa-restore {@link PropertyModel} with a {@link PwaRestoreBottomSheetView}.
 */
class PwaRestoreBottomSheetViewBinder {
    static void bind(PropertyModel model, PwaRestoreBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == PwaRestoreProperties.VIEW_STATE) {
            @ViewState
            int viewState = model.get(PwaRestoreProperties.VIEW_STATE);
            view.setDisplayedView(viewState);
        } else if (propertyKey.equals(PwaRestoreProperties.APPS)) {
            view.setAppList(
                    model.get(PwaRestoreProperties.APPS),
                    model.get(PwaRestoreProperties.RECENT_APPS_TITLE),
                    model.get(PwaRestoreProperties.OLDER_APPS_TITLE));
        } else if (propertyKey.equals(PwaRestoreProperties.PEEK_DESCRIPTION)) {
            ((TextView) view.getPreviewView().findViewById(R.id.description))
                    .setText(model.get(PwaRestoreProperties.PEEK_DESCRIPTION));
        } else if (propertyKey.equals(PwaRestoreProperties.PEEK_TITLE)) {
            ((TextView) view.getPreviewView().findViewById(R.id.title))
                    .setText(model.get(PwaRestoreProperties.PEEK_TITLE));
        } else if (propertyKey.equals(PwaRestoreProperties.PEEK_BUTTON_LABEL)) {
            ((Button) view.getPreviewView().findViewById(R.id.review_button))
                    .setText(model.get(PwaRestoreProperties.PEEK_BUTTON_LABEL));
        } else if (propertyKey.equals(PwaRestoreProperties.EXPANDED_DESCRIPTION)) {
            ((TextView) view.getContentView().findViewById(R.id.description))
                    .setText(model.get(PwaRestoreProperties.EXPANDED_DESCRIPTION));
        } else if (propertyKey.equals(PwaRestoreProperties.EXPANDED_TITLE)) {
            ((TextView) view.getContentView().findViewById(R.id.title))
                    .setText(model.get(PwaRestoreProperties.EXPANDED_TITLE));
        } else if (propertyKey.equals(PwaRestoreProperties.DESELECT_BUTTON_LABEL)) {
            ((Button) view.getContentView().findViewById(R.id.deselect_button))
                    .setText(model.get(PwaRestoreProperties.DESELECT_BUTTON_LABEL));
        } else if (propertyKey.equals(PwaRestoreProperties.EXPANDED_BUTTON_LABEL)) {
            ((Button) view.getContentView().findViewById(R.id.restore_button))
                    .setText(model.get(PwaRestoreProperties.EXPANDED_BUTTON_LABEL));
        } else if (propertyKey.equals(PwaRestoreProperties.BACK_BUTTON_ON_CLICK_CALLBACK)) {
            view.setBackButtonListener(
                    model.get(PwaRestoreProperties.BACK_BUTTON_ON_CLICK_CALLBACK));
        } else if (propertyKey.equals(PwaRestoreProperties.REVIEW_BUTTON_ON_CLICK_CALLBACK)) {
            ((Button) view.getPreviewView().findViewById(R.id.review_button))
                    .setOnClickListener(
                            model.get(PwaRestoreProperties.REVIEW_BUTTON_ON_CLICK_CALLBACK));
            ((Button) view.getPreviewView().findViewById(R.id.review_button)).setEnabled(true);
        } else if (propertyKey.equals(PwaRestoreProperties.DESELECT_BUTTON_ON_CLICK_CALLBACK)) {
            ((Button) view.getContentView().findViewById(R.id.deselect_button))
                    .setOnClickListener(
                            model.get(PwaRestoreProperties.DESELECT_BUTTON_ON_CLICK_CALLBACK));
            ((Button) view.getContentView().findViewById(R.id.deselect_button)).setEnabled(true);
        } else if (propertyKey.equals(PwaRestoreProperties.RESTORE_BUTTON_ON_CLICK_CALLBACK)) {
            ((Button) view.getContentView().findViewById(R.id.restore_button))
                    .setOnClickListener(
                            model.get(PwaRestoreProperties.RESTORE_BUTTON_ON_CLICK_CALLBACK));
            ((Button) view.getContentView().findViewById(R.id.restore_button)).setEnabled(true);
        }
    }
}
