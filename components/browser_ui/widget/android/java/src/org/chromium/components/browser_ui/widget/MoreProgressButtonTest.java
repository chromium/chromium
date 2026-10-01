// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.MoreProgressButton.State;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link MoreProgressButton}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MoreProgressButtonTest {
    private Activity mActivity;

    private MoreProgressButton mMoreProgressButton;
    private TextView mCustomTextView;

    private int mIdTextView;
    private int mIdMoreProgressButton;

    @Before
    public void setupTest() {
        var controller = Robolectric.buildActivity(TestActivity.class);
        controller.get().setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivity = controller.setup().get();
        FrameLayout contentView = new FrameLayout(mActivity);
        mActivity.setContentView(contentView);

        mIdTextView = View.generateViewId();
        mIdMoreProgressButton = View.generateViewId();

        mMoreProgressButton =
                (MoreProgressButton)
                        LayoutInflater.from(contentView.getContext())
                                .inflate(R.layout.more_progress_button, null);
        mMoreProgressButton.setId(mIdMoreProgressButton);
        contentView.addView(mMoreProgressButton, MATCH_PARENT, WRAP_CONTENT);

        mCustomTextView = new TextView(mActivity);
        mCustomTextView.setText("");
        mCustomTextView.setId(mIdTextView);
        contentView.addView(mCustomTextView, MATCH_PARENT, WRAP_CONTENT);
    }

    private void changeTextView(String newTextString) {
        mCustomTextView.setText(newTextString);
    }

    @Test
    @Feature({"MoreProgressButton"})
    public void testInitialStates() {
        // Verify the default status for the views are correct
        assertFalse(
                "Button should not be shown after init",
                mActivity.findViewById(R.id.action_button).isShown());
        assertFalse(
                "Spinner should not be shown after init",
                mActivity.findViewById(R.id.progress_spinner).isShown());
    }

    @Test
    @Feature({"MoreProgressButton"})
    public void testSetStateToButton() {
        mMoreProgressButton.setState(State.BUTTON);

        assertTrue(
                "Button should be shown with State.BUTTON",
                mActivity.findViewById(R.id.action_button).isShown());
        assertFalse(
                "Spinner should not be shown with State.BUTTON",
                mActivity.findViewById(R.id.progress_spinner).isShown());
    }

    @Test
    @Feature({"MoreProgressButton"})
    public void testSetStateToSpinner() {
        mMoreProgressButton.setState(State.LOADING);

        assertFalse(
                "Button should not be shown with State.LOADING",
                mActivity.findViewById(R.id.action_button).isShown());
        assertTrue(
                "Spinner should be shown with State.LOADING",
                mActivity.findViewById(R.id.progress_spinner).isShown());
    }

    @Test
    @Feature({"MoreProgressButton"})
    public void testSetStateToHidden() {
        // Change state for the button first, then hide it
        mMoreProgressButton.setState(State.BUTTON);
        mMoreProgressButton.setState(State.HIDDEN);

        assertFalse(
                "Button should not be shown with State.HIDDEN",
                mActivity.findViewById(R.id.action_button).isShown());
        assertFalse(
                "Spinner should not be shown with State.HIDDEN",
                mActivity.findViewById(R.id.progress_spinner).isShown());
    }

    @Test
    @Feature({"MoreProgressButton"})
    public void testStateAfterBindAction() {
        boolean buttonShownBefore = mActivity.findViewById(R.id.action_button).isShown();
        boolean spinnerShownBefore = mActivity.findViewById(R.id.progress_spinner).isShown();

        mMoreProgressButton.setOnClickRunnable(() -> changeTextView(""));

        assertEquals(
                "Button should stays same visibility before/after bind action",
                buttonShownBefore,
                mActivity.findViewById(R.id.action_button).isShown());
        assertEquals(
                "spinner should stays same visibility before/after bind action",
                spinnerShownBefore,
                mActivity.findViewById(R.id.progress_spinner).isShown());
    }

    @Test
    @Feature({"MoreProgressButton"})
    public void testClickAfterBindAction() {
        final String str = "Some Test String";

        String textViewStr = ((TextView) mActivity.findViewById(mIdTextView)).getText().toString();
        assertNotEquals(str, textViewStr);

        mMoreProgressButton.setOnClickRunnable(() -> changeTextView(str));
        mMoreProgressButton.setState(State.BUTTON);

        assertTrue(mActivity.findViewById(R.id.action_button).isClickable());

        mActivity.findViewById(R.id.action_button).performClick();

        assertFalse(mActivity.findViewById(R.id.action_button).isShown());
        assertTrue(mActivity.findViewById(R.id.progress_spinner).isShown());

        textViewStr = ((TextView) mActivity.findViewById(mIdTextView)).getText().toString();
        assertEquals(str, textViewStr);
    }
}
