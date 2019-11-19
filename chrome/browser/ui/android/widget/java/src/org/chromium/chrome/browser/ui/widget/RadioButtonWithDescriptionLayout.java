// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioGroup;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages a group of exclusive RadioButtonWithDescriptions, automatically inserting a margin in
 * between the rows to prevent them from squishing together. Has the option to set an accessory view
 * on any given RadioButtonWithDescription. Only one accessory view per layout is supported.
 *
 * -------------------------------------------------
 * | O | MESSAGE #1                                |
 *        description_1                            |
 *        [optional] accessory view                |
 * | O | MESSAGE #N                                |
 *        description_n                            |
 * -------------------------------------------------
 */
public final class RadioButtonWithDescriptionLayout
        extends RadioGroup implements RadioButtonWithDescription.ButtonCheckedStateChangedListener {
    /** Encapsulates information required to build a layout. */
    public static class Option {
        private final CharSequence mTitle;
        private final CharSequence mDescription;
        private final Object mTag;

        /**
         * @param title The title for this option. This will be displayed as the main text for the
         *              checkbox.
         * @param description The description for this option. This will be displayed as the subtext
         *                    under the main text for the checkbox.
         * @param tag The tag for this option. This can be used to identify the checkbox in event
         *            listeners.
         */
        public Option(CharSequence title, CharSequence description, @Nullable Object tag) {
            mTitle = title;
            mDescription = description;
            mTag = tag;
        }

        public CharSequence getTitle() {
            return mTitle;
        }

        public CharSequence getDescription() {
            return mDescription;
        }

        public Object getTag() {
            return mTag;
        }
    }

    private final int mMarginBetweenRows;
    private final List<RadioButtonWithDescription> mRadioButtonsWithDescriptions;
    private OnCheckedChangeListener mOnCheckedChangeListener;
    private View mAccessoryView;

    public RadioButtonWithDescriptionLayout(Context context) {
        this(context, null);
    }

    public RadioButtonWithDescriptionLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMarginBetweenRows = context.getResources().getDimensionPixelSize(
                R.dimen.default_vertical_margin_between_items);

        mRadioButtonsWithDescriptions = new ArrayList<>();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) getChildAt(i);
            setupButton(b);
        }

        updateMargins();
    }

    /**
     * @see RadioGroup.OnCheckedChangeListener
     */
    @Override
    public void setOnCheckedChangeListener(OnCheckedChangeListener onCheckedChangeListener) {
        mOnCheckedChangeListener = onCheckedChangeListener;
    }

    /**
     * @see RadioButtonWithDescription.ButtonCheckedStateChangedListener
     */
    @Override
    public void onButtonCheckedStateChanged(RadioButtonWithDescription checkedRadioButton) {
        mOnCheckedChangeListener.onCheckedChanged(this, checkedRadioButton.getId());
    }

    /**
     * Given a set of {@link Option} creates a set of {@link RadioButtonWithDescription}. When lists
     * are built this way, there are two options for getting the checkbox you want in an event
     * listener callback.
     * 1. Set a tag on the Option and use that with {@link View#findViewWithTag).
     * 2. Use the id passed through to {@link RadioGroup.OnCheckedChangeListener#onCheckedChanged}.
     *    with {@link View#findViewById}.
     *
     * @param options List of options to add to this group.
     */
    public void addOptions(List<Option> options) {
        for (Option option : options) {
            RadioButtonWithDescription b = new RadioButtonWithDescription(getContext(), null);
            b.setTitleText(option.getTitle());
            b.setDescriptionText(option.getDescription());
            b.setTag(option.getTag());

            setupButton(b);

            addView(b, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        updateMargins();
    }

    private View removeAttachedAccessoryView(View view) {
        // Remove the view from it's parent if it has one.
        if (view.getParent() != null) {
            ViewGroup previousGroup = (ViewGroup) view.getParent();
            previousGroup.removeView(view);
        }
        return view;
    }

    /**
     * Attach the given accessory view to the given RadioButtonWithDescription. The attachmentPoint
     * must be a direct child of this.
     *
     * @param accessoryView The accessory view to be attached.
     * @param attachmentPoint The RadioButtonWithDescription that the accessory view will be
     *                        attached to.
     */
    public void attachAccessoryView(
            View accessoryView, RadioButtonWithDescription attachmentPoint) {
        removeAttachedAccessoryView(accessoryView);
        int attachmentPointIndex = indexOfChild(attachmentPoint);
        assert attachmentPointIndex >= 0 : "attachmentPoint view must a child of layout.";
        addView(accessoryView, attachmentPointIndex + 1);
    }

    /** Sets margins between each of the radio buttons. */
    private void updateMargins() {
        int childCount = getChildCount();
        for (int i = 0; i < childCount - 1; i++) {
            View child = getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();
            params.bottomMargin = mMarginBetweenRows;
        }
        // LayoutParam changes only take effect after the next layout pass.
        requestLayout();
    }

    private void setupButton(RadioButtonWithDescription radioButton) {
        radioButton.setOnCheckedChangeListener(this);
        // Give the button a unique id to allow for calling onCheckedChanged (see
        // #onCheckedChanged).
        if (radioButton.getId() == NO_ID) radioButton.setId(generateViewId());
        radioButton.setRadioButtonGroup(mRadioButtonsWithDescriptions);
        mRadioButtonsWithDescriptions.add(radioButton);
    }

    /**
     * Marks a RadioButton child as being checked.
     *
     * @param childIndex Index of the child to select.
     */
    void selectChildAtIndexForTesting(int childIndex) {
        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) getChildAt(i);
            b.setChecked(i == childIndex);
        }
    }
}
