// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.prefeditor;

import android.content.Context;
import android.text.InputFilter;
import android.text.TextWatcher;
import android.view.View;
import android.widget.AutoCompleteTextView;
import android.widget.FrameLayout;
import android.widget.TextView.OnEditorActionListener;

import androidx.annotation.Nullable;

/** Handles validation and display of one field from the {@link EditorFieldModel}. */
// TODO(b/173103628): Re-enable this
//@VisibleForTesting
public class EditorTextField extends FrameLayout implements EditorFieldView, View.OnClickListener {

    private AutoCompleteTextView mInput;

    public EditorTextField(Context context, final EditorFieldModel fieldModel,
            OnEditorActionListener actionListener, @Nullable InputFilter filter,
            @Nullable TextWatcher formatter) {
        super(context);
        mInput = new AutoCompleteTextView(context);
        addView(mInput);
    }

    @Override
    public void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);

        if (hasWindowFocus) updateFieldValueIcon(true);
    }

    @Override
    public void onClick(View v) {

    }

    /** @return The AutoCompleteTextView this field associates*/
    public AutoCompleteTextView getEditText() {
        return mInput;
    }

    @Override
    public boolean isValid() {
        return false;
    }

    @Override
    public boolean isRequired() {
        return false;
    }

    @Override
    public void updateDisplayedError(boolean showError) {

    }

    @Override
    public void scrollToAndFocus() {

    }

    @Override
    public void update() {

    }

    private void updateFieldValueIcon(boolean force) {

    }

}
