// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.app.Activity;
import android.content.DialogInterface;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.autofill_assistant.AssistantTextUtils;
import org.chromium.components.autofill_assistant.LayoutUtils;
import org.chromium.components.autofill_assistant.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * Shows a link for the user to open an information dialog on the origin of their data.
 */
public class AssistantDataOriginNotice {
    private static final int ACCOUNT_SETTINGS_SCREEN_ID = 10003;

    private final View mView;
    private final TextView mLinkToDataOriginDialog;
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private String mDialogTitle = "";
    private String mDialogText = "";
    private String mDialogButtonText = "";
    private String mAccountEmail = "";

    AssistantDataOriginNotice(Activity activity, WindowAndroid windowAndroid, ViewGroup parent) {
        mActivity = activity;
        mView = LayoutUtils.createInflater(activity).inflate(
                R.layout.autofill_assistant_data_origin_notice, parent, false);
        parent.addView(mView);
        mLinkToDataOriginDialog = mView.findViewById(R.id.link_to_data_origin_dialog);
        mWindowAndroid = windowAndroid;

        // Hide the section initially until required to be shown.
        mView.setVisibility(View.GONE);
    }

    void setLinkText(String text) {
        if (TextUtils.isEmpty(text)) {
            mView.setVisibility(View.GONE);
            return;
        }

        mView.setVisibility(View.VISIBLE);

        mLinkToDataOriginDialog.setText(text);
        ApiCompatibilityUtils.setTextAppearance(
                mLinkToDataOriginDialog, R.style.TextAppearance_TextSmall_Link);
        mLinkToDataOriginDialog.setOnClickListener(this::showDataOriginDialog);
    }

    void setDialogTitle(String title) {
        mDialogTitle = title;
    }

    void setDialogText(String text) {
        mDialogText = text;
    }

    void setDialogButtonText(String text) {
        mDialogButtonText = text;
    }

    void setAccountEmail(String accountEmail) {
        mAccountEmail = accountEmail;
    }

    View getView() {
        return mView;
    }

    private void onDataOriginLinkClicked(int unusedLink) {
        new GmsIntegrator(mAccountEmail, mActivity)
                .launchAccountIntent(ACCOUNT_SETTINGS_SCREEN_ID, mWindowAndroid, unused -> {});
    }

    private void showDataOriginDialog(View unusedView) {
        AlertDialog dialog = createAlertDialog();
        dialog.show();

        // Make links in the dialog clickable.
        ((TextView) dialog.findViewById(android.R.id.message))
                .setMovementMethod(LinkMovementMethod.getInstance());
    }

    private AlertDialog createAlertDialog() {
        return new AlertDialog.Builder(mActivity, R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setTitle(mDialogTitle)
                .setMessage(AssistantTextUtils.applyVisualAppearanceTags(
                        mDialogText, mActivity, this::onDataOriginLinkClicked))
                .setPositiveButton(
                        mDialogButtonText, (DialogInterface dialogInterface, int unused) -> {})
                .create();
    }
}
