// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;

/** View for rendering illustration card items in the @memory bottom sheet list. */
@NullMarked
public class AtMemoryBottomSheetIllustrationCardView extends LinearLayout {
    private TextView mTitleView;
    private TextView mSubtitleView;

    public AtMemoryBottomSheetIllustrationCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.illustration_card_title);
        mSubtitleView = findViewById(R.id.illustration_card_subtitle);
    }

    public void setTitle(@Nullable String title) {
        mTitleView.setText(title);
        mTitleView.setVisibility(TextUtils.isEmpty(title) ? GONE : VISIBLE);
    }

    public void setSubtitle(@Nullable String subtitle) {
        mSubtitleView.setText(subtitle);
        mSubtitleView.setVisibility(TextUtils.isEmpty(subtitle) ? GONE : VISIBLE);
    }
}
