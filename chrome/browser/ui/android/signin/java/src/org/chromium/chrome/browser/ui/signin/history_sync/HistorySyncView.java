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
    private Button mMoreButton;
    private Button mAcceptButton;
    private DualControlLayout mButtonBar;

    public HistorySyncView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // TODO(crbug.com/1520791): Set up scrollView.
        mAccountImage = findViewById(R.id.account_image);
        TextView title = findViewById(R.id.sync_consent_title);
        TextView subtitle = findViewById(R.id.sync_consent_subtitle);

        mAcceptButton = findViewById(R.id.positive_button);
        mDeclineButton = findViewById(R.id.negative_button);
        if (mAcceptButton == null) {
            // The landscape layout does not have accept and decline buttons. Create the button bar.
            createButtonBar();
        }

        mMoreButton = findViewById(R.id.more_button);
        TextView detailsDescription = findViewById(R.id.sync_consent_details_description);

        // TODO(crbug.com/1520791): Confirm that these are the correct title and subtitle strings.
        // Using group C from the strings variation experiment as a placeholder in the meantime.
        title.setText(R.string.history_sync_consent_title_c);
        subtitle.setText(R.string.history_sync_consent_subtitle_c);
        mDeclineButton.setText(R.string.no_thanks);
        mMoreButton.setText(R.string.more);
        mAcceptButton.setText(R.string.signin_accept_button);
        detailsDescription.setText(R.string.sync_consent_details_description);
    }

    ImageView getAccountImageView() {
        return mAccountImage;
    }

    Button getDeclineButton() {
        return mDeclineButton;
    }

    Button getMoreButton() {
        return mMoreButton;
    }

    Button getAcceptButton() {
        return mAcceptButton;
    }

    void createButtonBarForTablets() {
        mAcceptButton.setVisibility(GONE);
        mDeclineButton.setVisibility(GONE);
        createButtonBar();
        mDeclineButton.setText(R.string.no_thanks);
        mAcceptButton.setText(R.string.signin_accept_button);
        mButtonBar.setVisibility(VISIBLE);
    }

    private void createButtonBar() {
        mAcceptButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), DualControlLayout.ButtonType.PRIMARY_FILLED, "", null);
        mDeclineButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), DualControlLayout.ButtonType.SECONDARY, "", null);
        mButtonBar = findViewById(R.id.dual_control_button_bar);
        mButtonBar.addView(mAcceptButton);
        mButtonBar.addView(mDeclineButton);
        mButtonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.END);
    }
}
