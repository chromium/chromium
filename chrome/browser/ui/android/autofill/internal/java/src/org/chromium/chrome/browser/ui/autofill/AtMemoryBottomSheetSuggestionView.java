// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;

/** View for an individual suggestion in the AtMemory bottom sheet. */
@NullMarked
public class AtMemoryBottomSheetSuggestionView extends LinearLayout {
    private static final float GRAYED_OUT_OPACITY_ALPHA = 0.38f;
    private static final float COMPLETE_OPACITY_ALPHA = 1.0f;

    private ImageView mIconView;
    private TextView mTitleView;
    private TextView mDetailsView;
    private View mArrowView;
    private View mDividerView;

    public AtMemoryBottomSheetSuggestionView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = findViewById(R.id.icon_view);
        mTitleView = findViewById(R.id.title_text);
        mDetailsView = findViewById(R.id.details_text);
        mArrowView = findViewById(R.id.arrow_view);
        mDividerView = findViewById(R.id.divider_view);
    }

    public void setIcon(int resId) {
        mIconView.setImageResource(resId);
    }

    public void setTitle(@Nullable String text) {
        mTitleView.setText(text);
    }

    public void setDetails(@Nullable String text) {
        mDetailsView.setText(text);
        mDetailsView.setVisibility(TextUtils.isEmpty(text) ? View.GONE : View.VISIBLE);
    }

    public void setSuggestionClickListener(Runnable callback) {
        setOnClickListener(v -> callback.run());
    }

    public void setFlyoutClickListener(Runnable callback) {
        mArrowView.setOnClickListener(v -> callback.run());
    }

    public void setFlyoutVisible(boolean visible) {
        mArrowView.setVisibility(visible ? View.VISIBLE : View.GONE);
        mDividerView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    // TODO(crbug.com/534668890): Implement the state pattern for the view.
    public void applyDeactivatedStyle(boolean applyDeactivatedStyle) {
        if (applyDeactivatedStyle) {
            this.setEnabled(false);
            mTitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
            mDetailsView.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
            mIconView.setAlpha(GRAYED_OUT_OPACITY_ALPHA);
        } else {
            this.setEnabled(true);
            mTitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
            mDetailsView.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
            mIconView.setAlpha(COMPLETE_OPACITY_ALPHA);
        }
    }
}
