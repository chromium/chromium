// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.signin.MinorModeHelper.ScreenMode;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.browser_ui.widget.DualControlLayout;

/** View that wraps history sync consent screen and caches references to UI elements. */
class HistorySyncView extends LinearLayout {
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

        if (isButtonBar) {
            createButtonBar(restrictionStatus);
        } else {
            createButtonsForPortraitLayout(restrictionStatus);
        }
        assert mAcceptButton != null && mDeclineButton != null;
        mAcceptButton.setText(R.string.history_sync_primary_action);
        mDeclineButton.setText(R.string.history_sync_secondary_action);

        mAcceptButton.setVisibility(VISIBLE);
        mDeclineButton.setVisibility(VISIBLE);
    }

    private void createButtonBar(@ScreenMode int restrictionStatus) {
        DualControlLayout buttonBar = findViewById(R.id.dual_control_button_bar);

        final @DualControlLayout.ButtonType int acceptButtonType;
        final @DualControlLayout.ButtonType int declineButtonType;
        if (restrictionStatus == ScreenMode.UNRESTRICTED) {
            acceptButtonType = DualControlLayout.ButtonType.PRIMARY_FILLED;
            declineButtonType = DualControlLayout.ButtonType.SECONDARY;
        } else {
            acceptButtonType = DualControlLayout.ButtonType.PRIMARY_OUTLINED;
            declineButtonType = DualControlLayout.ButtonType.SECONDARY_OUTLINED;
        }

        mAcceptButton =
                DualControlLayout.createButtonForLayout(getContext(), acceptButtonType, "", null);
        mDeclineButton =
                DualControlLayout.createButtonForLayout(getContext(), declineButtonType, "", null);

        buttonBar.addView(mAcceptButton);
        buttonBar.addView(mDeclineButton);
        buttonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.END);
        buttonBar.setVisibility(VISIBLE);
    }

    private void createButtonsForPortraitLayout(@ScreenMode int restrictionStatus) {
        // TODO(b/345663992) Allow buttons to be added dynamically
        if (restrictionStatus == ScreenMode.UNRESTRICTED) {
            mAcceptButton = findViewById(R.id.button_primary);
            mDeclineButton = findViewById(R.id.button_secondary);
        } else {
            mAcceptButton = findViewById(R.id.button_primary_minor_mode);
            mDeclineButton = findViewById(R.id.button_secondary_minor_mode);
        }
    }
}
