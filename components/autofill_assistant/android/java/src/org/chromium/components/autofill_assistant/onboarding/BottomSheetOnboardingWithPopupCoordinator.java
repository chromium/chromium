// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.onboarding;

import android.content.Context;
import android.content.DialogInterface;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.AssistantInfoPageUtil;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.util.AccessibilityUtil;
import org.chromium.ui.widget.ButtonCompat;

import java.util.Map;

/**
 * Similar to the regular bottom sheet onboarding, this experimental onboarding coordinator shows
 * the terms and conditions in a popup dialog, rather than directly in the bottom sheet.
 */
public class BottomSheetOnboardingWithPopupCoordinator extends BottomSheetOnboardingCoordinator {
    private static final String SPLIT_ONBOARDING_TERMS_TITLE_KEY = "split_onboarding_terms_title";
    private static final String SPLIT_ONBOARDING_CLOSE_BOTTOMSHEET_KEY =
            "split_onboarding_close_bottomsheet";
    private static final String SPLIT_ONBOARDING_SHOW_DIALOG_KEY = "split_onboarding_show_dialog";
    private static final String SPLIT_ONBOARDING_ACCEPT_DIALOG_KEY = "split_onboarding_accept";
    private static final String SPLIT_ONBOARDING_CLOSE_DIALOG_KEY = "split_onboarding_decline";
    private static final String SPLIT_ONBOARDING_TITLE_KEY = "split_onboarding_title";
    private static final String SPLIT_ONBOARDING_SUBTITLE_KEY = "split_onboarding_text";
    // We have a bit more space in the dialog, so we add line spacing to make it easier to read the
    // terms.
    private static final float TERMS_LINE_SPACING_MULTIPLIER = 1.25f;

    private @Nullable AlertDialog mDialog;

    BottomSheetOnboardingWithPopupCoordinator(BrowserContextHandle browserContext,
            AssistantInfoPageUtil infoPageUtil, String experimentIds,
            Map<String, String> parameters, Context context, BottomSheetController controller,
            AssistantBrowserControlsFactory browserControlsFactory, View rootView,
            ScrimCoordinator scrim, AccessibilityUtil accessibilityUtil) {
        super(browserContext, infoPageUtil, experimentIds, parameters, context, controller,
                browserControlsFactory, rootView, scrim, accessibilityUtil);
    }

    @Override
    ScrollView createViewImpl() {
        ScrollView view = super.createViewImpl();

        // Terms will be shown in the dialog instead.
        view.findViewById(R.id.onboarding_separator).setVisibility(View.GONE);
        view.findViewById(R.id.terms_spacing_before).setVisibility(View.GONE);
        view.findViewById(R.id.onboarding_terms).setVisibility(View.GONE);
        // Note: the spacing after the terms serves as separator to the buttons, so it must remain
        // visible.
        view.findViewById(R.id.terms_spacing_after).setVisibility(View.VISIBLE);

        return view;
    }

    @Override
    protected void setupSharedView(Callback<Integer> callback) {
        super.setupSharedView(callback);

        // Override the behavior for the 'ok' button in the bottom sheet: it should open the terms
        // dialog and not accept the onboarding directly.
        mView.findViewById(R.id.button_init_ok).setOnClickListener(unused -> showDialog(callback));
    }

    @Override
    public void hide() {
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
        super.hide();
    }

    @Override
    public void updateViews() {
        assert mView != null;

        ButtonCompat bottomSheetYesButton = mView.findViewById(R.id.button_init_ok);
        if (mStringMap.containsKey(SPLIT_ONBOARDING_SHOW_DIALOG_KEY)) {
            bottomSheetYesButton.setText(mStringMap.get(SPLIT_ONBOARDING_SHOW_DIALOG_KEY));
        } else {
            bottomSheetYesButton.setText(mContext.getApplicationContext().getString(
                    R.string.autofill_assistant_split_onboarding_show_dialog));
        }
        bottomSheetYesButton.setContentDescription(bottomSheetYesButton.getText());

        ButtonCompat bottomSheetNoButton = mView.findViewById(R.id.button_init_not_ok);
        if (mStringMap.containsKey(SPLIT_ONBOARDING_CLOSE_BOTTOMSHEET_KEY)) {
            bottomSheetNoButton.setText(mStringMap.get(SPLIT_ONBOARDING_CLOSE_BOTTOMSHEET_KEY));
        } else {
            bottomSheetNoButton.setText(mContext.getApplicationContext().getString(
                    R.string.autofill_assistant_split_onboarding_close_bottomsheet));
        }
        bottomSheetNoButton.setContentDescription(bottomSheetNoButton.getText());

        TextView titleView = mView.findViewById(R.id.onboarding_try_assistant);
        if (mStringMap.containsKey(SPLIT_ONBOARDING_TITLE_KEY)) {
            titleView.setText(mStringMap.get(SPLIT_ONBOARDING_TITLE_KEY));
        } else {
            titleView.setText(mContext.getApplicationContext().getString(
                    R.string.autofill_assistant_split_onboarding_title));
        }
        TextView subtitleView = mView.findViewById(R.id.onboarding_subtitle);
        if (mStringMap.containsKey(SPLIT_ONBOARDING_SUBTITLE_KEY)) {
            subtitleView.setText(mStringMap.get(SPLIT_ONBOARDING_SUBTITLE_KEY));
        } else {
            subtitleView.setText(mContext.getApplicationContext().getString(
                    R.string.autofill_assistant_split_onboarding_subtitle));
        }
    }

    private void showDialog(Callback<Integer> callback) {
        View dialogView = super.createViewImpl();
        // Hide the views that are already shown in mView or otherwise unnecessary.
        dialogView.findViewById(R.id.onboarding_subtitle_container).setVisibility(View.GONE);
        dialogView.findViewById(R.id.onboarding_separator).setVisibility(View.GONE);
        dialogView.findViewById(R.id.terms_spacing_after).setVisibility(View.GONE);
        dialogView.findViewById(R.id.button_init_ok).setVisibility(View.GONE);
        dialogView.findViewById(R.id.button_init_not_ok).setVisibility(View.GONE);

        // Use a slightly larger and easier to read text appearance for the terms in the dialog.
        TextView termsView = dialogView.findViewById(R.id.google_terms_message);
        ApiCompatibilityUtils.setTextAppearance(
                termsView, R.style.TextAppearance_TextMedium_Primary);
        termsView.setLineSpacing(/* add = */ 0.0f, /* mult = */ TERMS_LINE_SPACING_MULTIPLIER);

        // Update dialog strings from string map.
        TextView termsTitleView = dialogView.findViewById(R.id.onboarding_try_assistant);
        if (mStringMap.containsKey(SPLIT_ONBOARDING_TERMS_TITLE_KEY)) {
            termsTitleView.setText(mStringMap.get(SPLIT_ONBOARDING_TERMS_TITLE_KEY));
        } else {
            termsTitleView.setText(mContext.getApplicationContext().getString(
                    R.string.autofill_assistant_split_onboarding_terms_title));
        }
        updateTermsAndConditionsView(dialogView.findViewById(R.id.google_terms_message));

        String acceptDialog = mStringMap.get(SPLIT_ONBOARDING_ACCEPT_DIALOG_KEY);
        if (acceptDialog == null) {
            acceptDialog = mContext.getString(R.string.init_ok);
        }
        String cancelDialog = mStringMap.get(SPLIT_ONBOARDING_CLOSE_DIALOG_KEY);
        if (cancelDialog == null) {
            cancelDialog = mContext.getString(R.string.cancel);
        }

        mDialog = new AlertDialog.Builder(getContext(), R.style.Theme_Chromium_AlertDialog)
                          .setPositiveButton(acceptDialog,
                                  (DialogInterface dialog, int unused) -> {
                                      // Hide the dialog, but don't hide the bottom sheet content,
                                      // to allow for smooth transition into the flow.
                                      dialog.dismiss();
                                      mDialog = null;
                                      callback.onResult(AssistantOnboardingResult.ACCEPTED);
                                  })
                          .setNegativeButton(cancelDialog,
                                  (DialogInterface dialog, int unused) -> {
                                      dialog.dismiss();
                                      mDialog = null;
                                  })
                          .create();
        mDialog.setView(dialogView);
        mDialog.show();
    }
}
