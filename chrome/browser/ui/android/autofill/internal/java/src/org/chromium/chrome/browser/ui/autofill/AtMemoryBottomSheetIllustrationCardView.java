// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;

import java.util.List;
import java.util.Random;

/** View for rendering illustration card items in the AtMemory bottom sheet list. */
@NullMarked
public class AtMemoryBottomSheetIllustrationCardView extends LinearLayout {
    static final long TITLE_ROTATION_INTERVAL_MS = 3000L;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Random mRandom = new Random();

    private TextView mTitleView;
    private TextView mSubtitleView;
    private List<String> mRotatingPlaceholders = List.of();
    private int mNextRotatingPlaceholderIndex;
    private boolean mIsRotating;

    public AtMemoryBottomSheetIllustrationCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.illustration_card_title);
        mSubtitleView = findViewById(R.id.illustration_card_subtitle);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        startPlaceholderRotation();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        stopPlaceholderRotation();
    }

    public void setTitle(@Nullable String title) {
        mTitleView.setText(title);
        mTitleView.setVisibility(TextUtils.isEmpty(title) ? GONE : VISIBLE);
    }

    public void setSubtitle(@Nullable String subtitle) {
        mSubtitleView.setText(subtitle);
        mSubtitleView.setVisibility(TextUtils.isEmpty(subtitle) ? GONE : VISIBLE);
    }

    public void setRotatingPlaceholders(List<String> rotatingPlaceholders) {
        mRotatingPlaceholders = rotatingPlaceholders;
        stopPlaceholderRotation();
        startPlaceholderRotation();
    }

    private void startPlaceholderRotation() {
        if (mRotatingPlaceholders.isEmpty() || mIsRotating) {
            return;
        }
        mIsRotating = true;
        mNextRotatingPlaceholderIndex = mRandom.nextInt(mRotatingPlaceholders.size());
        mHandler.postDelayed(this::setPlaceholderText, TITLE_ROTATION_INTERVAL_MS);
    }

    private void stopPlaceholderRotation() {
        if (!mIsRotating) {
            return;
        }
        mIsRotating = false;
        mHandler.removeCallbacksAndMessages(null);
    }

    private void setPlaceholderText() {
        if (mRotatingPlaceholders.isEmpty()) {
            return;
        }
        setTitle(mRotatingPlaceholders.get(mNextRotatingPlaceholderIndex));
        mNextRotatingPlaceholderIndex =
                (mNextRotatingPlaceholderIndex + 1) % mRotatingPlaceholders.size();
        mHandler.postDelayed(this::setPlaceholderText, TITLE_ROTATION_INTERVAL_MS);
    }
}
