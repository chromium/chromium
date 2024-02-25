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

import org.chromium.chrome.browser.ui.signin.ConsentTextTracker;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.widget.ButtonCompat;

/** View that wraps history sync consent screen and caches references to UI elements. */
class HistorySyncView extends LinearLayout {
    private ImageView mAccountImage;
    private Button mDeclineButton;
    private Button mMoreButton;
    private ButtonCompat mAcceptButton;
    private TextView mDetailsDescription;

    private ConsentTextTracker mConsentTextTracker;

    public HistorySyncView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mConsentTextTracker = new ConsentTextTracker(getResources());
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // TODO(crbug.com/1520791): Set up scrollView.
        mAccountImage = findViewById(R.id.account_image);
        TextView title = findViewById(R.id.sync_consent_title);
        TextView subtitle = findViewById(R.id.sync_consent_subtitle);
        mDeclineButton = findViewById(R.id.negative_button);
        mMoreButton = findViewById(R.id.more_button);
        mAcceptButton = findViewById(R.id.positive_button);
        mDetailsDescription = findViewById(R.id.sync_consent_details_description);

        // TODO(crbug.com/1520791): Confirm that these are the correct title and subtitle strings.
        // Using group C from the strings variation experiment as a placeholder in the meantime.
        mConsentTextTracker.setText(title, R.string.history_sync_consent_title_c);
        mConsentTextTracker.setText(subtitle, R.string.history_sync_consent_subtitle_c);
        mConsentTextTracker.setText(mDeclineButton, R.string.no_thanks);
        mConsentTextTracker.setText(mMoreButton, R.string.more);
        mConsentTextTracker.setText(mAcceptButton, R.string.signin_accept_button);
        mConsentTextTracker.setText(mDetailsDescription, R.string.sync_consent_details_description);
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

    ButtonCompat getAcceptButton() {
        return mAcceptButton;
    }

    TextView getDetailsDescriptionView() {
        return mDetailsDescription;
    }
}
