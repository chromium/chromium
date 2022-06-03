// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.javascript_dialogs;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

/**
 * The JavaScript dialog that is either app modal or tab modal.
 */
public class JavascriptDialogCustomView extends LinearLayout {
    private EditText mPromptEditText;
    private CheckBox mSuppressCheckBox;

    /**
     * Constructor for inflating from XMLs.
     */
    public JavascriptDialogCustomView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPromptEditText = findViewById(R.id.js_modal_dialog_prompt);
        mSuppressCheckBox = findViewById(R.id.suppress_js_modal_dialogs);
    }

    /**
     * @param promptText Prompt text for prompt dialog. If null, prompt text is not visible.
     */
    public void setPromptText(String promptText) {
        if (promptText == null) return;
        mPromptEditText.setVisibility(View.VISIBLE);

        if (promptText.length() > 0) {
            mPromptEditText.setText(promptText);
            mPromptEditText.selectAll();
        }
    }

    /**
     * @return The prompt text edited by user.
     */
    public String getPromptText() {
        return mPromptEditText.getText().toString();
    }

    /**
     * @param visible Whether the suppress check box should be visible. The check box should only
     *                be set visible if applicable for app modal JavaScript dialogs.
     */
    public void setSuppressCheckBoxVisibility(boolean visible) {
        mSuppressCheckBox.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /**
     * @return Whether the suppress check box is checked by user.
     */
    public boolean isSuppressCheckBoxChecked() {
        return mSuppressCheckBox.isChecked();
    }
}
