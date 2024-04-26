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
        TextView title = findViewById(R.id.sync_consent_title);
        TextView subtitle = findViewById(R.id.sync_consent_subtitle);
        mDetailsDescription = findViewById(R.id.sync_consent_details_description);

        // TODO(crbug.com/41493766): Confirm that these are the correct title and subtitle strings.
        // Using group C from the strings variation experiment as a placeholder in the meantime.
        title.setText(R.string.history_sync_consent_title_c);
        subtitle.setText(R.string.history_sync_consent_subtitle_c);
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

    void createButtons(boolean isButtonBar) {
        if (isButtonBar) {
            createButtonBar();
        } else {
            mAcceptButton = findViewById(R.id.button_primary);
            mDeclineButton = findViewById(R.id.button_secondary);
            mAcceptButton.setVisibility(VISIBLE);
            mDeclineButton.setVisibility(VISIBLE);
        }
        assert mAcceptButton != null && mDeclineButton != null;
        mAcceptButton.setText(R.string.signin_accept_button);
        mDeclineButton.setText(R.string.no_thanks);
    }

    private void createButtonBar() {
        mAcceptButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), DualControlLayout.ButtonType.PRIMARY_FILLED, "", null);
        mDeclineButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), DualControlLayout.ButtonType.SECONDARY, "", null);
        DualControlLayout buttonBar = findViewById(R.id.dual_control_button_bar);
        buttonBar.addView(mAcceptButton);
        buttonBar.addView(mDeclineButton);
        buttonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.END);
        buttonBar.setVisibility(VISIBLE);
    }
}
