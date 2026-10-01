// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

/** Unit tests for {@link RadioButtonWithDescriptionAndAuxButton}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RadioButtonWithDescriptionAndAuxButtonTest {

    private static class AuxButtonClickedListener
            implements RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private int mClickedId;

        AuxButtonClickedListener() {}

        @Override
        public void onAuxButtonClicked(int clickedId) {
            mCallbackHelper.notifyCalled();
            mClickedId = clickedId;
        }

        int getTimesCalled() {
            return mCallbackHelper.getCallCount();
        }

        int getClickedId() {
            return mClickedId;
        }
    }

    private AuxButtonClickedListener mListener;
    private RadioButtonWithDescriptionAndAuxButton mRadioButton;

    @Before
    public void setupTest() {
        mListener = new AuxButtonClickedListener();
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        ViewGroup contentView = new FrameLayout(context);
        View layout =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.radio_button_with_description_and_aux_button_test,
                                contentView,
                                false);
        contentView.addView(layout);

        mRadioButton = layout.findViewById(R.id.test_radio_button);
        assertNotNull(mRadioButton);
    }

    @Test
    public void testOnAuxButtonClicked() {
        mRadioButton.setAuxButtonClickedListener(mListener);
        mRadioButton.getAuxButtonForTests().performClick();
        assertEquals(
                "AuxButtonClickedListener#onAuxButtonClicked should be called once",
                1,
                mListener.getTimesCalled());
        assertEquals(R.id.test_radio_button, mListener.getClickedId());
    }

    @Test
    public void testAuxButtonEnabled() {
        mRadioButton.setEnabled(false);
        assertFalse(
                "Primary TextView should be set to disabled.",
                mRadioButton.getPrimaryTextView().isEnabled());
        assertFalse(
                "Description TextView should be set to disabled.",
                mRadioButton.getDescriptionTextView().isEnabled());
        assertFalse(
                "RadioButton should be set to disabled.",
                mRadioButton.getRadioButtonView().isEnabled());
        assertFalse(
                "Aux Button should be set to disabled.",
                mRadioButton.getAuxButtonForTests().isEnabled());
        mRadioButton.setAuxButtonEnabled(true);
        assertFalse(
                "Primary TextView should keep disabled.",
                mRadioButton.getPrimaryTextView().isEnabled());
        assertFalse(
                "Description TextView should keep disabled.",
                mRadioButton.getDescriptionTextView().isEnabled());
        assertFalse(
                "RadioButton should keep disabled.", mRadioButton.getRadioButtonView().isEnabled());
        assertTrue(
                "Aux Button should be set to enabled.",
                mRadioButton.getAuxButtonForTests().isEnabled());
    }

    @Test
    public void testPaddingAndBackgroundValue() {
        View radioContainer = mRadioButton.findViewById(R.id.radio_container);
        int lateralPadding =
                mRadioButton
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.radio_button_with_description_lateral_padding);
        int auxButtonSpacing =
                mRadioButton
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.radio_button_with_description_and_aux_button_spacing);
        assertEquals(
                "Lateral padding should be set in the radio container.",
                lateralPadding,
                radioContainer.getPaddingStart());
        assertEquals(
                "Aux button spacing should be set in the radio container.",
                auxButtonSpacing,
                radioContainer.getPaddingEnd());
        assertEquals(
                "Lateral padding should be set to 0 in the radio button root layout.",
                0,
                mRadioButton.getPaddingStart());
        assertNotNull(
                "Background should be set in the radio container.", radioContainer.getBackground());
        assertNull(
                "Background should be null in the radio button root layout",
                mRadioButton.getBackground());
    }
}
