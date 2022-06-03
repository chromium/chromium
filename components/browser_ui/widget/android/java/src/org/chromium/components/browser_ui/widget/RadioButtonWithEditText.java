// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.ArrayList;
import java.util.List;

/**
 * <p>
 * A radio button that contains a text edit box. The text value inside the entry box could used to
 * represent the value when this radio button is selected. This class also supports the
 * functionality of adding a description the same as {@link RadioButtonWithDescription}.
 *
 * By default, this class is inflated from {@link R.layout.radio_button_with_edit_text}.
 * </p>
 *
 * <p>
 * There is no default hint provided in the EditText. User could set the hint message through {@link
 * #setHint} API, or through android:hint attribute in xml definition.
 * </p>
 *
 * <p>
 * This class also provides an interface {@link RadioButtonWithEditText.OnLongClickListener} to
 * observe the text changing in its entry box. To use, implement the interface {@link
 * RadioButtonWithEditText.OnLongClickListener} and call {@link
 * RadioButtonWithEditText#addTextChangeListener(OnTextChangeListener)} to start listening to
 * changes in the EditText.
 * </p>
 *
 * <p>
 * The input type, text, hint message of EditText box and an optional description to be contained in
 * the group may be set in XML. Sample declaration in XML:
 * <pre>{@code
 *  <org.chromium.components.browser_ui.widget.RadioButtonWithEditText
 *     android:id="@+id/system_default"
 *     android:layout_width="match_parent"
 *     android:layout_height="wrap_content"
 *     android:background="?attr/selectableItemBackground"
 *     android:inputType="text"
 *     android:hint="@string/hint_text_bar"
 *     app:primaryText="@string/feature_foo_option_one"
 *     app:descriptionText="@string/feature_foo_option_one_description" />
 * }</pre>
 * </p>
 */
public class RadioButtonWithEditText extends RadioButtonWithDescription {
    /**
     * Interface that will subscribe to changes to the text inside {@link RadioButtonWithEditText}.
     *
     */
    public interface OnTextChangeListener {
        /**
         * Will be called when the EditText has a value change.
         * @param newText The updated text in EditText.
         */
        void onTextChanged(CharSequence newText);
    }

    private EditText mEditText;
    private List<OnTextChangeListener> mListeners;

    public RadioButtonWithEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
        mListeners = new ArrayList<>();
    }

    @Override
    protected int getLayoutResource() {
        return R.layout.radio_button_with_edit_text;
    }

    @Override
    protected void setViewsInternal() {
        super.setViewsInternal();

        mEditText = (EditText) getPrimaryTextView();
        mEditText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                // Text set through AttributionSet will trigger this function before mListeners
                // initialize, so we only notify listeners after initialization.
                if (mListeners == null) return;

                for (OnTextChangeListener listener : mListeners) {
                    listener.onTextChanged(s);
                }
            }
        });

        // Handles keyboard actions
        mEditText.setOnEditorActionListener((v, actionId, event) -> {
            mEditText.clearFocus();
            return false;
        });

        // Handle touches beside the Edit text
        mEditText.setOnFocusChangeListener((v, hasFocus) -> { onEditTextFocusChanged(hasFocus); });
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        // Fix the announcement for a11y as EditText cannot be correctly read out as a child
        // of a ViewGroup. Setting EditText as the label for this custom view is a workaround
        // as label will be announce at end of ViewGroup's readable a11y children.
        super.onInitializeAccessibilityNodeInfo(info);
        info.setLabeledBy(mEditText);
    }

    @Override
    protected TextView getPrimaryTextView() {
        return findViewById(R.id.edit_text);
    }

    @Override
    protected void applyAttributes(AttributeSet attrs) {
        super.applyAttributes(attrs);

        TypedArray a = getContext().getTheme().obtainStyledAttributes(
                attrs, R.styleable.RadioButtonWithEditText, 0, 0);

        String hint = a.getString(R.styleable.RadioButtonWithEditText_android_hint);
        if (hint != null) setHint(hint);

        int inputType = a.getInt(
                R.styleable.RadioButtonWithEditText_android_inputType, InputType.TYPE_CLASS_TEXT);
        setInputType(inputType);

        a.recycle();
    }

    /**
     * Sets the checked status.
     */
    @Override
    public void setChecked(boolean checked) {
        super.setChecked(checked);
        mEditText.clearFocus();
    }

    private void onEditTextFocusChanged(boolean hasFocus) {
        if (hasFocus) {
            setCheckedWithNoFocusChange(true);
            mEditText.setCursorVisible(true);
        } else {
            mEditText.setCursorVisible(false);
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(mEditText);
        }
    }

    /**
     * Add a listener that will be notified when text inside this url has been changed
     * @param listener New listener that will be notified when text edit has been changed
     */
    public void addTextChangeListener(OnTextChangeListener listener) {
        mListeners.add(listener);
    }

    /**
     * Remove the listener from the subscription list
     * @param listener Listener that will no longer listening to text edit changes
     */
    public void removeTextChangeListener(OnTextChangeListener listener) {
        mListeners.remove(listener);
    }

    /**
     * Set the input type of text editor
     * @param inputType An input type from {@link android.text.InputType}
     */
    public void setInputType(int inputType) {
        mEditText.setInputType(inputType);
    }

    /**
     * Set the hint message of text edit box
     */
    public void setHint(CharSequence hint) {
        mEditText.setHint(hint);
    }

    /**
     * Set the hint message of text edit box using pre-defined string from {@link R.string}
     */
    public void setHint(int hintId) {
        mEditText.setHint(hintId);
    }

    /**
     * @return the EditText living inside this widget.
     */
    @VisibleForTesting
    public EditText getEditTextForTests() {
        return mEditText;
    }
}
