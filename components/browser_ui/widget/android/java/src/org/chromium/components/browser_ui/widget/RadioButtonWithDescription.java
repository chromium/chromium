// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewStub;
import android.widget.RadioButton;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.ChromeImageView;

import java.util.List;

/**
 * <p>
 * A RadioButton with a primary and descriptive text to the right.
 * The radio button is designed to be contained in a group, with {@link
 * RadioButtonWithDescriptionLayout} as the parent view. By default, the object will be inflated
 * from {@link R.layout.radio_button_with_description).
 * </p>
 *
 * <p>
 * A child widget can replace the end_view_stub ViewStub with a customized view at the end of the
 * widget, by overriding {@link RadioButtonWithDescription#getEndStubLayoutResourceId()}.
 * </p>
 *
 * <p>
 * By default, R.attr.selectableItemBackground will be set as the background. If a different
 * background is desired, use android:background to override.
 * </p>
 *
 * <p>
 * The primary of the text and an optional description to be contained in the group may be set in
 * XML. Sample declaration in XML:
 * <pre> {@code
 *   <org.chromium.components.browser_ui.widget.RadioButtonWithDescription
 *      android:id="@+id/system_default"
 *      android:layout_width="match_parent"
 *      android:layout_height="wrap_content"
 *      app:iconSrc="@drawable/ic_foo"    <-- optional -->
 *      app:primaryText="@string/feature_foo_option_one"
 *      app:descriptionText="@string/feature_foo_option_one_description" />
 * } </pre>
 * </p>
 */
public class RadioButtonWithDescription extends RelativeLayout implements OnClickListener {
    /** Interface to listen to radio button changes. */
    public interface ButtonCheckedStateChangedListener {
        /**
         * Invoked when a {@link RadioButtonWithDescription} is selected.
         * @param checkedRadioButton The radio button that was selected.
         */
        void onButtonCheckedStateChanged(RadioButtonWithDescription checkedRadioButton);
    }

    private RadioButton mRadioButton;
    private ChromeImageView mIcon;
    private TextView mPrimary;
    private TextView mDescription;

    private ButtonCheckedStateChangedListener mButtonCheckedStateChangedListener;

    private List<RadioButtonWithDescription> mGroup;

    private static final String SUPER_STATE_KEY = "superState";
    private static final String CHECKED_KEY = "isChecked";
    // An id that indicates the layout doesn't exist.
    private static final int NO_LAYOUT_ID = -1;

    /** Constructor for inflating via XML. */
    public RadioButtonWithDescription(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(getLayoutResource(), this, true);

        setViewsInternal();

        if (attrs != null) applyAttributes(attrs);

        setMinimumHeight(getResources().getDimensionPixelSize(R.dimen.min_touch_target_size));

        final int lateralPadding =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.radio_button_with_description_lateral_padding);
        final int verticalPadding =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.radio_button_with_description_vertical_padding);
        // allow override
        setPaddingRelative(
                getPaddingStart() == 0 ? lateralPadding : getPaddingStart(),
                getPaddingTop() == 0 ? verticalPadding : getPaddingTop(),
                getPaddingEnd() == 0 ? lateralPadding : getPaddingEnd(),
                getPaddingBottom() == 0 ? verticalPadding : getPaddingBottom());

        // Set the background if not specified in xml
        if (getBackground() == null) {
            TypedValue background = new TypedValue();
            getContext()
                    .getTheme()
                    .resolveAttribute(android.R.attr.selectableItemBackground, background, true);
            if (getEndStubLayoutResourceId() != NO_LAYOUT_ID) {
                // If the end view stub is replaced with a custom view, only set background in the
                // button container, so the end view is not highlighted when the button is clicked.
                View radioContainer = findViewById(R.id.radio_container);
                radioContainer.setBackgroundResource(background.resourceId);
                // Move the start padding into radio container, so it can be highlighted.
                int paddingStart = getPaddingStart();
                radioContainer.setPaddingRelative(
                        paddingStart,
                        radioContainer.getPaddingTop(),
                        radioContainer.getPaddingEnd(),
                        radioContainer.getPaddingBottom());
                setPaddingRelative(0, getPaddingTop(), getPaddingEnd(), getPaddingBottom());
            } else {
                setBackgroundResource(background.resourceId);
            }
        }

        // We want RadioButtonWithDescription to handle the clicks itself.
        setOnClickListener(this);
        // Make it focusable for navigation via key events (tab/up/down keys)
        // with Bluetooth keyboard. See: crbug.com/936143
        setFocusable(true);
    }

    /** Set the view elements that included in xml internally. */
    protected void setViewsInternal() {
        mRadioButton = getRadioButtonView();
        mIcon = getIcon();
        mPrimary = getPrimaryTextView();
        mDescription = getDescriptionTextView();

        int endStubLayoutResourceId = getEndStubLayoutResourceId();
        if (endStubLayoutResourceId != NO_LAYOUT_ID) {
            ViewStub endStub = findViewById(R.id.end_view_stub);
            endStub.setLayoutResource(endStubLayoutResourceId);
            endStub.inflate();
        }
    }

    /** @return The layout resource id used for inflating this {@link RadioButtonWithDescription}. */
    protected int getLayoutResource() {
        return R.layout.radio_button_with_description;
    }

    /** @return RadioButton View inside this {@link RadioButtonWithDescription}. */
    protected RadioButton getRadioButtonView() {
        return (RadioButton) findViewById(R.id.radio_button);
    }

    /** @return ChromeImageView inside this {@link RadioButtonWithDescription}. */
    protected ChromeImageView getIcon() {
        return (ChromeImageView) findViewById(R.id.icon);
    }

    /** @return TextView displayed as primary inside this {@link RadioButtonWithDescription}. */
    protected TextView getPrimaryTextView() {
        return (TextView) findViewById(R.id.primary);
    }

    /** @return TextView displayed as description inside this {@link RadioButtonWithDescription}. */
    protected TextView getDescriptionTextView() {
        return (TextView) findViewById(R.id.description);
    }

    /**
     * @return Resource id that is used to replace the end_view_stub inside this {@link
     *         RadioButtonWithDescription}.
     */
    protected int getEndStubLayoutResourceId() {
        return NO_LAYOUT_ID;
    }

    /**
     * Apply the customized AttributeSet to current view.
     * @param attrs AttributeSet that will be applied to current view.
     */
    protected void applyAttributes(AttributeSet attrs) {
        TypedArray a =
                getContext()
                        .getTheme()
                        .obtainStyledAttributes(
                                attrs, R.styleable.RadioButtonWithDescription, 0, 0);

        Drawable iconDrawable =
                UiUtils.getDrawable(
                        getContext(), a, R.styleable.RadioButtonWithDescription_iconSrc);

        if (iconDrawable != null) {
            mIcon.setImageDrawable(iconDrawable);
            mIcon.setVisibility(View.VISIBLE);
        }

        String primaryText = a.getString(R.styleable.RadioButtonWithDescription_primaryText);
        if (primaryText != null) mPrimary.setText(primaryText);

        String descriptionText =
                a.getString(R.styleable.RadioButtonWithDescription_descriptionText);
        if (descriptionText != null) {
            mDescription.setText(descriptionText);
            mDescription.setVisibility(View.VISIBLE);
        } else {
            ((LayoutParams) mPrimary.getLayoutParams()).addRule(RelativeLayout.CENTER_VERTICAL);
        }

        a.recycle();
    }

    @Override
    public void onClick(View v) {
        setChecked(true);

        if (mButtonCheckedStateChangedListener != null) {
            mButtonCheckedStateChangedListener.onButtonCheckedStateChanged(this);
        }
    }

    /** Sets the text shown in the primary section. */
    public void setPrimaryText(CharSequence text) {
        mPrimary.setText(text);
    }

    /** @return The text shown in the primary section. */
    public CharSequence getPrimaryText() {
        return mPrimary.getText();
    }

    /** Sets the text shown in the description section. */
    public void setDescriptionText(CharSequence text) {
        mDescription.setText(text);

        if (TextUtils.isEmpty(text)) {
            ((LayoutParams) mPrimary.getLayoutParams()).addRule(RelativeLayout.CENTER_VERTICAL);
            mDescription.setVisibility(View.GONE);
        } else {
            ((LayoutParams) mPrimary.getLayoutParams()).removeRule(RelativeLayout.CENTER_VERTICAL);
            mDescription.setVisibility(View.VISIBLE);
        }
    }

    /** @return The text shown in the description section. */
    public CharSequence getDescriptionText() {
        return mDescription.getText();
    }

    /** Returns true if checked. */
    public boolean isChecked() {
        return mRadioButton.isChecked();
    }

    /**
     * Sets the checked status, and retain focus on RadioButtonWithDescription after radio button if
     * it is checked.
     *
     * If the radio button is inside a radio button group and going to be checked, the rest of the
     * radio buttons in the group will be set to unchecked.
     *
     * @param checked Whether this radio button will be checked.
     */
    public void setChecked(boolean checked) {
        setCheckedWithNoFocusChange(checked);
        // Retain focus on RadioButtonWithDescription after radio button is checked.
        // Otherwise focus is lost. This is required for Bluetooth keyboard navigation.
        // See: crbug.com/936143
        if (checked) requestFocus();
    }

    /**
     * Set the checked status for this radio button without updating the focus.
     *
     * If the radio button is inside a radio button group and going to be checked, the rest of the
     * radio buttons in the group will be set to unchecked by #setChecked(false).
     *
     * In most cases, caller should use {@link #setChecked(boolean)} to handle the focus as well.
     * @param checked Whether this radio button will be checked.
     */
    protected void setCheckedWithNoFocusChange(boolean checked) {
        if (mGroup != null && checked) {
            for (RadioButtonWithDescription button : mGroup) {
                if (button != this) button.setChecked(false);
            }
        }
        mRadioButton.setChecked(checked);
    }

    public void setOnCheckedChangeListener(ButtonCheckedStateChangedListener listener) {
        mButtonCheckedStateChangedListener = listener;
    }

    /**
     * Sets the group of RadioButtonWithDescriptions that should be unchecked when this button is
     * checked.
     * @param group A list containing all elements of the group.
     */
    public void setRadioButtonGroup(List<RadioButtonWithDescription> group) {
        mGroup = group;
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);

        mDescription.setEnabled(enabled);
        mPrimary.setEnabled(enabled);
        mRadioButton.setEnabled(enabled);
        if (mIcon != null) {
            TypedValue disabledAlpha = new TypedValue();
            getContext()
                    .getResources()
                    .getValue(R.dimen.default_disabled_alpha, disabledAlpha, true);
            mIcon.setAlpha(enabled ? 1.0f : disabledAlpha.getFloat());
        }
    }

    @Override
    protected Parcelable onSaveInstanceState() {
        // Since this View is designed to be used multiple times in the same layout and contains
        // children with ids, Android gets confused. This is because it will see two Views in the
        // hierarchy with the same id (eg, the RadioButton that makes up this View).
        //
        // eg:
        // LinearLayout (no id):
        // |-> RadioButtonWithDescription (id=sync_confirm_import_choice)
        // |   |-> RadioButton            (id=radio_button)
        // |   |-> TextView               (id=primary)
        // |   \-> TextView               (id=description)
        // \-> RadioButtonWithDescription (id=sync_keep_separate_choice)
        //     |-> RadioButton            (id=radio_button)
        //     |-> TextView               (id=primary)
        //     \-> TextView               (id=description)
        //
        // This causes the automagic state saving and recovery to do the wrong thing and restore all
        // of these Views to the state of the last one it saved.
        // Therefore we manually save the state of the child Views here so the state can be
        // associated with the id of the RadioButtonWithDescription, which should be unique and
        // not the id of the RadioButtons, which will be duplicated.
        //
        // Note: We disable Activity recreation on many config changes (such as orientation),
        // but not for all of them (eg, locale or font scale change). So this code will only be
        // called on the latter ones, or when the Activity is destroyed due to memory constraints.
        Bundle saveState = new Bundle();
        saveState.putParcelable(SUPER_STATE_KEY, super.onSaveInstanceState());
        saveState.putBoolean(CHECKED_KEY, isChecked());
        return saveState;
    }

    @Override
    protected void onRestoreInstanceState(Parcelable state) {
        if (state instanceof Bundle) {
            super.onRestoreInstanceState(((Bundle) state).getParcelable(SUPER_STATE_KEY));
            setChecked(((Bundle) state).getBoolean(CHECKED_KEY));
        } else {
            super.onRestoreInstanceState(state);
        }
    }

    @Override
    protected void dispatchSaveInstanceState(SparseArray<Parcelable> container) {
        // This method and dispatchRestoreInstanceState prevent the Android automagic state save
        // and restore from touching this View's children.
        dispatchFreezeSelfOnly(container);
    }

    @Override
    protected void dispatchRestoreInstanceState(SparseArray<Parcelable> container) {
        dispatchThawSelfOnly(container);
    }
}
