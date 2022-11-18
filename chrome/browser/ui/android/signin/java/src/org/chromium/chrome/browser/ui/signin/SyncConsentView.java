// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.ui.widget.ButtonCompat;

/** View that wraps sync consent screen and caches references to UI elements. */
class SyncConsentView extends LinearLayout {
    private SigninScrollView mScrollView;
    private ImageView mAccountImage;
    private TextView mTitle;
    private TextView mSubtitle;
    private TextView mBookmarksRow;
    private TextView mAutofillRow;
    private TextView mHistoryRow;
    private Button mRefuseButton;
    private Button mMoreButton;
    private ButtonCompat mAcceptButton;
    private TextView mDetailsDescription;

    public SyncConsentView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = findViewById(R.id.sync_consent_scroll_view);
        mAccountImage = findViewById(R.id.account_image);
        mTitle = findViewById(R.id.sync_consent_title);
        mSubtitle = findViewById(R.id.sync_consent_subtitle);
        mBookmarksRow = findViewById(R.id.bookmarks_row);
        mAutofillRow = findViewById(R.id.autofill_row);
        mHistoryRow = findViewById(R.id.history_row);
        mRefuseButton = findViewById(R.id.negative_button);
        mMoreButton = findViewById(R.id.more_button);
        mAcceptButton = findViewById(R.id.positive_button);
        mDetailsDescription = findViewById(R.id.sync_consent_details_description);
    }

    SigninScrollView getScrollView() {
        return mScrollView;
    }

    ImageView getAccountImageView() {
        return mAccountImage;
    }

    TextView getTitleView() {
        return mTitle;
    }

    TextView getSubtitleView() {
        return mSubtitle;
    }

    TextView getBookmarksRow() {
        return mBookmarksRow;
    }

    TextView getAutofillRow() {
        return mAutofillRow;
    }

    TextView getHistoryRow() {
        return mHistoryRow;
    }

    Button getRefuseButton() {
        return mRefuseButton;
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
