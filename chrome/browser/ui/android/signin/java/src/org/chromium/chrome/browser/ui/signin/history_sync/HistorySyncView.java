// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.signin.MinorModeHelper.ScreenMode;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.browser_ui.widget.DualControlLayout.DualControlLayoutAlignment;

/** View that wraps history sync consent screen and caches references to UI elements. */
public class HistorySyncView extends LinearLayout {
    private ImageView mAccountImage;
    private Button mDeclineButton;
    private Button mAcceptButton;
    private TextView mDetailsDescription;

    public HistorySyncView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // TODO(crbug.com/41493766): Set up scrollView.
        mAccountImage = findViewById(R.id.history_sync_account_image);
        mDetailsDescription = findViewById(R.id.history_sync_footer);
    }

    ImageView getAccountImageView() {
        return mAccountImage;
    }

    Button getDeclineButton() {
        return mDeclineButton;
    }

    Button getAcceptButton() {
        return mAcceptButton;
    }

    TextView getDetailsDescription() {
        return mDetailsDescription;
    }

    /**
     * Creates buttons for history sync screen only when {@link
     * org.chromium.chrome.browser.ui.signin.MinorModeHelper} has resolved
     *
     * @param isButtonBar Should view use a buttons bar
     * @param restrictionStatus Indicates if MinorModeHelper has resolved and if minor mode
     *     restrictions apply
     */
    void maybeCreateButtons(boolean isButtonBar, @ScreenMode int restrictionStatus) {
        if (restrictionStatus == ScreenMode.PENDING) {
            // Do not create buttons if MinorModeHelper has not resolved
            return;
        }

        createButtons(restrictionStatus, isButtonBar);
    }

    private void createButtons(@ScreenMode int restrictionStatus, boolean isButtonBar) {

        final @DualControlLayout.ButtonType int acceptButtonType;
        final @DualControlLayout.ButtonType int declineButtonType;
        if (restrictionStatus == ScreenMode.UNRESTRICTED) {
            acceptButtonType = DualControlLayout.ButtonType.PRIMARY_FILLED;
            declineButtonType = DualControlLayout.ButtonType.SECONDARY_TEXT;
        } else {
            acceptButtonType = DualControlLayout.ButtonType.PRIMARY_OUTLINED;
            declineButtonType = DualControlLayout.ButtonType.SECONDARY_OUTLINED;
        }

        mAcceptButton =
                DualControlLayout.createButtonForLayout(getContext(), acceptButtonType, "", null);
        mDeclineButton =
                DualControlLayout.createButtonForLayout(getContext(), declineButtonType, "", null);

        // In certain situations (e.g. a wide screen) the accept and refuse buttons will be placed
        // on either ends of the screen using the DualControlLayout. When the buttons should be
        // stacked the buttons are added to a LinearLayout (R.id.small_screen_button_layout).
        if (isButtonBar) {
            DualControlLayout dualControlButtonBar = findViewById(R.id.dual_control_button_bar);
            dualControlButtonBar.removeAllViews();

            dualControlButtonBar.addView(mAcceptButton);
            dualControlButtonBar.addView(mDeclineButton);
            dualControlButtonBar.setAlignment(DualControlLayoutAlignment.END);
            dualControlButtonBar.setVisibility(VISIBLE);
        } else {
            LinearLayout smallScreenButtonLayout = findViewById(R.id.small_screen_button_layout);
            smallScreenButtonLayout.removeAllViews();

            ViewGroup.LayoutParams layoutParams =
                    new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);
            mAcceptButton.setLayoutParams(layoutParams);
            mDeclineButton.setLayoutParams(layoutParams);

            smallScreenButtonLayout.addView(mAcceptButton);
            smallScreenButtonLayout.addView(mDeclineButton);
            smallScreenButtonLayout.setVisibility(VISIBLE);
        }

        assert mAcceptButton != null && mDeclineButton != null;
        mAcceptButton.setText(R.string.history_sync_primary_action);
        mDeclineButton.setText(R.string.history_sync_secondary_action);

        mAcceptButton.setVisibility(VISIBLE);
        mDeclineButton.setVisibility(VISIBLE);
    }
}
