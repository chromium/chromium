// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** The Mediator for the PWA Restore bottom sheet. */
@JNINamespace("webapk")
class PwaRestoreBottomSheetMediator {
    // The current activity.
    private final Activity mActivity;

    // The underlying property model for the bottom sheeet.
    private final PropertyModel mModel;

    // The callback for the parent to get notified on when Restore is clicked.
    private final Runnable mParentRestoreClickHandler;

    private long mNativeMediator;

    PwaRestoreBottomSheetMediator(
            ArrayList apps,
            Activity activity,
            Runnable onReviewButtonClicked,
            Runnable onRestoreButtonClicked,
            Runnable onBackButtonClicked) {
        mActivity = activity;
        mParentRestoreClickHandler = onRestoreButtonClicked;
        mModel =
                PwaRestoreProperties.createModel(
                        onReviewButtonClicked,
                        onBackButtonClicked,
                        this::onDeselectButtonClicked,
                        this::onRestoreButtonClicked,
                        this::onSelectionToggled);
        mNativeMediator = PwaRestoreBottomSheetMediatorJni.get().initialize(this);

        initializeState(apps);
        setPeekingState();
    }

    private void initializeState(ArrayList apps) {
        mModel.set(
                PwaRestoreProperties.PEEK_TITLE,
                mActivity.getString(R.string.pwa_restore_title_peeking));
        mModel.set(
                PwaRestoreProperties.PEEK_DESCRIPTION,
                mActivity.getString(R.string.pwa_restore_description_peeking));
        mModel.set(
                PwaRestoreProperties.PEEK_BUTTON_LABEL,
                mActivity.getString(R.string.pwa_restore_button_peeking));

        mModel.set(
                PwaRestoreProperties.EXPANDED_TITLE,
                mActivity.getString(R.string.pwa_restore_title_expanded));
        mModel.set(
                PwaRestoreProperties.EXPANDED_DESCRIPTION,
                mActivity.getString(R.string.pwa_restore_description_expanded));
        mModel.set(
                PwaRestoreProperties.APPS_TITLE,
                mActivity.getString(R.string.pwa_restore_apps_list));
        mModel.set(
                PwaRestoreProperties.EXPANDED_BUTTON_LABEL,
                mActivity.getString(R.string.pwa_restore_button_expanded));
        mModel.set(
                PwaRestoreProperties.DESELECT_BUTTON_LABEL,
                mActivity.getString(R.string.pwa_restore_button_deselect));

        mModel.set(PwaRestoreProperties.APPS, apps);
    }

    protected void setPeekingState() {
        mModel.set(PwaRestoreProperties.VIEW_STATE, ViewState.PREVIEW);
    }

    protected void setPreviewState() {
        mModel.set(PwaRestoreProperties.VIEW_STATE, ViewState.VIEW_PWA_LIST);
    }

    private void onDeselectButtonClicked() {
        List<PwaRestoreProperties.AppInfo> appList = mModel.get(PwaRestoreProperties.APPS);
        // Deselect all apps.
        for (PwaRestoreProperties.AppInfo app : appList) {
            if (app.isSelected()) app.toggleSelection();
        }
        mModel.set(PwaRestoreProperties.APPS, appList);
        mModel.set(PwaRestoreProperties.DESELECT_BUTTON_ENABLED, false);
        mModel.set(PwaRestoreProperties.EXPANDED_BUTTON_ENABLED, false);
    }

    private void onRestoreButtonClicked() {
        List<PwaRestoreProperties.AppInfo> appList = mModel.get(PwaRestoreProperties.APPS);
        List<String> selectedAppLists = new ArrayList();
        for (PwaRestoreProperties.AppInfo app : appList) {
            if (app.isSelected()) {
                selectedAppLists.add(app.getId());
            }
        }
        if (mNativeMediator != 0) {
            PwaRestoreBottomSheetMediatorJni.get()
                    .onRestoreWebapps(
                            mNativeMediator,
                            selectedAppLists.toArray(new String[selectedAppLists.size()]));
        }

        // Notify the parent.
        mParentRestoreClickHandler.run();
    }

    private void onSelectionToggled(View view) {
        String appId = (String) view.getTag();

        List<PwaRestoreProperties.AppInfo> appList = mModel.get(PwaRestoreProperties.APPS);

        boolean somethingSelected = false;
        for (PwaRestoreProperties.AppInfo app : appList) {
            if (TextUtils.equals(app.getId(), appId)) {
                app.toggleSelection();
            }

            if (app.isSelected()) {
                somethingSelected = true;
            }
        }

        mModel.set(PwaRestoreProperties.DESELECT_BUTTON_ENABLED, somethingSelected);
        mModel.set(PwaRestoreProperties.EXPANDED_BUTTON_ENABLED, somethingSelected);
    }

    PropertyModel getModel() {
        return mModel;
    }

    @NativeMethods
    interface Natives {
        long initialize(PwaRestoreBottomSheetMediator instance);

        void onRestoreWebapps(long nativePwaRestoreBottomSheetMediator, String[] restoreAppsList);

        void destroy(long nativePwaRestoreBottomSheetMediator);
    }
}
