// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;

/**
 * <p>
 * A RadioButton with a primary and descriptive text to the right, and an aux button at the end.
 * The radio button is designed to be contained in a group, with {@link
 * RadioButtonWithDescriptionLayout} as the parent view. By default, the object will be inflated
 * from {@link R.layout.radio_button_with_description} and the end_view_stub ViewStub will
 * be replaced with {@link R.layout.expand_arrow_with_separator}.
 * </p>
 *
 * <p>
 * Setting the enabled state of the aux button alone is supported. By default, when {@link
 * RadioButtonWithDescriptionAndAuxButton#setEnabled(boolean)} is called, the enabled state of all
 * child views are set, including the aux button. Callers can call {@link
 * RadioButtonWithDescriptionAndAuxButton#setAuxButtonEnabled(boolean)} to set the enabled
 * state of the aux button independently. Note that if setEnabled is called after
 * setAuxButtonEnabled is called, the state of the aux button will be overridden.
 * </p>
 *
 * <p>
 * This class also provides an interface {@link
 * RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener} to observe the aux button
 * clicked event. To use, implement the interface {@link
 * RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener} and call {@link
 * RadioButtonWithDescriptionAndAuxButton#setAuxButtonClickedListener
 * (RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener)}
 * to start listening to the aux button clicked event on the aux button.
 * </p>
 *
 * <p>
 * The primary of the text and an optional description to be contained in the group may be set in
 * XML. Sample declaration in XML:
 * <pre>{@code
 *  <org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton
 *  *      android:id="@+id/system_default"
 *  *      android:layout_width="match_parent"
 *  *      android:layout_height="wrap_content"
 *  *      android:background="?attr/selectableItemBackground"
 *  *      app:primaryText="@string/feature_foo_option_one"
 *  *      app:descriptionText="@string/feature_foo_option_one_description" />
 * }</pre>
 * </p>
 *
 */
public class RadioButtonWithDescriptionAndAuxButton extends RadioButtonWithDescription {
    /**
     * Interface that will subscribe to aux button clicked event inside {@link
     * RadioButtonWithDescriptionAndAuxButton}.
     *
     */
    public interface OnAuxButtonClickedListener {
        /**
         * Notify that the button is clicked.
         * @param clickedButtonId The id of the radio button as a whole, not the id of the aux
         *         button.
         */
        void onAuxButtonClicked(int clickedButtonId);
    }

    private OnAuxButtonClickedListener mListener;
    private ImageButton mAuxButton;

    public RadioButtonWithDescriptionAndAuxButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Clear any end padding set by default in the parent class, since end padding
        // is built into the aux button instead.
        setPaddingRelative(getPaddingStart(), getPaddingTop(), 0, getPaddingBottom());

        View radioContainer = findViewById(R.id.radio_container);
        // Space between the radio container and the separator. The padding is added in the radio
        // container instead of the separator, because the padding needs to be highlighted when the
        // radio container is clicked.
        final int radioContainerEndPadding =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.radio_button_with_description_and_aux_button_spacing);
        radioContainer.setPaddingRelative(
                radioContainer.getPaddingStart(),
                radioContainer.getPaddingTop(),
                radioContainerEndPadding,
                radioContainer.getPaddingBottom());
    }

    @Override
    protected void setViewsInternal() {
        super.setViewsInternal();
        mAuxButton = findViewById(R.id.expand_arrow);
        getPrimaryTextView().setLabelFor(mAuxButton.getId());
    }

    /**
     * The end stub layout resource id is currently hardcoded. It can be made configurable in the
     * future if there is a need.
     */
    @Override
    protected int getEndStubLayoutResourceId() {
        return R.layout.expand_arrow_with_separator;
    }

    /**
     * Sets the enabled state of all child views, including the aux button.
     * @param enabled The enabled state of this view.
     */
    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        setAuxButtonEnabled(enabled);
    }

    /**
     * Sets the enabled state of the aux button alone. This can be used if you want the aux button
     * to be enabled while the other child views are disabled or vice versa.
     * @param enabled The enabled state of the aux button.
     */
    public void setAuxButtonEnabled(boolean enabled) {
        mAuxButton.setEnabled(enabled);
    }

    /**
     * Set a listener that will be notified when the aux button is clicked.
     * @param listener New listener that will be notified when the aux button is clicked.
     */
    public void setAuxButtonClickedListener(OnAuxButtonClickedListener listener) {
        mListener = listener;
        mAuxButton.setOnClickListener(v -> mListener.onAuxButtonClicked(getId()));
    }

    /** @return the aux button living inside this widget. */
    public ImageButton getAuxButtonForTests() {
        return mAuxButton;
    }
}
