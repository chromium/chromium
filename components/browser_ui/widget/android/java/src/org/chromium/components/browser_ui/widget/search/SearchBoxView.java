// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.search;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
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

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    private void setupView() {

        mSearchText = findViewById(R.id.search_text);
        mClearButton = findViewById(R.id.clear_text_button);
        mSearchLoupe = findViewById(R.id.search_loupe);

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
                        mSearchText.clearFocus();
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
                mSearchText.requestFocus();
            } else {
                mSearchText.clearFocus();
            }
        } finally {
            mIsSettingFocus = false;
        }
    }
}
