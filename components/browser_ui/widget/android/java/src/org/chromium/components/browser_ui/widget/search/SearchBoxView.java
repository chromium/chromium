// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.search;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.text.Editable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.text.EmptyTextWatcher;

/**
 * The view for the generic search box. It embeds the search text, the clear button, and maintains
 * listener references cleanly without polluting the view binder.
 */
@NullMarked
public class SearchBoxView extends LinearLayout {
    private EditText mSearchText;
    private View mClearButton;
    private View mSearchLoupe;
    private boolean mIsSettingText;
    private boolean mIsSettingFocus;

    private @Nullable Callback<String> mSearchTextCallback;
    private @Nullable Callback<Boolean> mFocusChangeCallback;
    private @Nullable Runnable mClearButtonClickedRunnable;

    /** Constructor for inflation. */
    @SuppressWarnings("NullAway.Init")
    public SearchBoxView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.search_box_view_layout, this, true);
        setupView();
    }

    // Suppress ClickableViewAccessibility because onTouchListener returns false, delegating click
    // handling and performClick calls to EditText's native onTouchEvent.
    @SuppressLint("ClickableViewAccessibility")
    private void setupView() {
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        setDesktopMode(false);

        mSearchText = findViewById(R.id.search_text);
        mClearButton = findViewById(R.id.clear_text_button);
        mSearchLoupe = findViewById(R.id.search_loupe);

        // The search text is focusable for keyboard navigation (Tab key) and TalkBack, but
        // should not be focusable in touch mode until explicitly activated. This prevents
        // Android's ViewRootImpl from auto-focusing the search box on startup/attachment
        // when no hardware keyboard is present.
        mSearchText.setFocusable(true);
        mSearchText.setFocusableInTouchMode(false);

        mSearchText.setOnTouchListener(
                (v, event) -> {
                    int action = event.getActionMasked();
                    if (action == MotionEvent.ACTION_DOWN) {
                        mSearchText.setFocusableInTouchMode(true);
                    } else if (action == MotionEvent.ACTION_CANCEL && !mSearchText.hasFocus()) {
                        mSearchText.setFocusableInTouchMode(false);
                    }
                    return false;
                });

        mSearchText.setOnClickListener(
                (v) -> {
                    if (!mSearchText.hasFocus()) {
                        mSearchText.setFocusableInTouchMode(true);
                        mSearchText.requestFocus();
                    }
                });

        mSearchText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (mIsSettingText) return;
                        if (mSearchTextCallback != null) {
                            mSearchTextCallback.onResult(s.toString());
                        }
                    }
                });

        mSearchText.setOnFocusChangeListener(
                (v, hasFocus) -> {
                    mSearchText.setFocusableInTouchMode(hasFocus);
                    if (mIsSettingFocus) return;
                    if (mFocusChangeCallback != null) {
                        mFocusChangeCallback.onResult(hasFocus);
                    }
                });

        mSearchText.setOnEditorActionListener(
                (TextView v, int actionId, KeyEvent event) -> {
                    if (actionId == EditorInfo.IME_ACTION_SEARCH
                            || (event != null
                                    && event.getAction() == KeyEvent.ACTION_DOWN
                                    && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                        KeyboardVisibilityDelegate.getInstance().hideKeyboard(mSearchText);
                        setSearchTextFocus(false);
                        return true;
                    }
                    return false;
                });

        mClearButton.setOnClickListener(
                (v) -> {
                    if (mClearButtonClickedRunnable != null) {
                        mClearButtonClickedRunnable.run();
                    }
                });
    }

    /** Sets the text without triggering the text watcher. */
    public void setSearchText(@Nullable String text) {
        if (TextUtils.equals(text, mSearchText.getText().toString())) return;

        mIsSettingText = true;
        try {
            mSearchText.setText(text);
        } finally {
            mIsSettingText = false;
        }
    }

    /** Sets the hint text. */
    public void setHintText(@Nullable String hint) {
        mSearchText.setHint(hint);
    }

    /** Sets the callback for search queries. */
    public void setSearchTextCallback(@Nullable Callback<String> callback) {
        mSearchTextCallback = callback;
    }

    /** Sets the callback for focus changes. */
    public void setFocusChangeCallback(@Nullable Callback<Boolean> callback) {
        mFocusChangeCallback = callback;
    }

    /** Sets the runnable for clear clicks. */
    public void setClearButtonClickedRunnable(@Nullable Runnable runnable) {
        mClearButtonClickedRunnable = runnable;
    }

    /** Sets visibility of clear button. */
    public void setClearButtonVisibility(boolean visible) {
        mClearButton.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets visibility of the search loupe icon. */
    public void setSearchLoupeVisibility(int visibility) {
        if (mSearchLoupe != null) {
            mSearchLoupe.setVisibility(visibility);
        }
    }

    /** Requests or clears focus for the text box. */
    public void setSearchTextFocus(boolean hasFocus) {
        if (hasFocus == mSearchText.hasFocus()) return;

        mIsSettingFocus = true;
        try {
            if (hasFocus) {
                mSearchText.setFocusableInTouchMode(true);
                mSearchText.requestFocus();
            } else {
                mSearchText.setFocusableInTouchMode(false);
                mSearchText.clearFocus();
            }
        } finally {
            mIsSettingFocus = false;
        }
    }

    /**
     * Updates the component's internal properties (padding, background, height) for desktop
     * constraints.
     */
    public void setDesktopMode(boolean isDesktop) {
        Resources res = getContext().getResources();

        int paddingEndPx =
                res.getDimensionPixelSize(
                        isDesktop
                                ? R.dimen.search_box_view_padding_horizontal_desktop
                                : R.dimen.search_box_view_padding_end_default);
        int paddingStartPx = res.getDimensionPixelSize(R.dimen.search_box_view_padding_start);
        int backgroundRes =
                isDesktop ? R.drawable.search_box_background : R.drawable.search_row_modern_bg;

        setPaddingRelative(paddingStartPx, getPaddingTop(), paddingEndPx, getPaddingBottom());
        setBackgroundResource(backgroundRes);

        int heightPx =
                res.getDimensionPixelSize(
                        isDesktop
                                ? R.dimen.search_box_view_height_desktop
                                : R.dimen.search_box_view_height_default);
        ViewGroup.LayoutParams params = getLayoutParams();
        if (params != null) {
            params.height = heightPx;
            setLayoutParams(params);
        } else {
            setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, heightPx));
        }
    }
}
