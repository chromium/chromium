// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ui.widget.MoreProgressButton.State;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link MoreProgressButton}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class MoreProgressButtonTest extends DummyUiActivityTestCase {
    private FrameLayout mContentView;
    private MoreProgressButton mMoreProgressButton;
    private TextView mCustomTextView;
    private Activity mActivity;

    private int mIdTextView;
    private int mIdMoreProgressButton;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mActivity = getActivity();

        setUpViews();
    }

    private void setUpViews() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new FrameLayout(mActivity);
            mActivity.setContentView(mContentView);

            mIdTextView = View.generateViewId();
            mIdMoreProgressButton = View.generateViewId();

            mMoreProgressButton =
                    (MoreProgressButton) LayoutInflater.from(mContentView.getContext())
                            .inflate(R.layout.more_progress_button, null);
            mMoreProgressButton.setId(mIdMoreProgressButton);
            mContentView.addView(mMoreProgressButton, MATCH_PARENT, WRAP_CONTENT);

            mCustomTextView = new TextView(mActivity);
            mCustomTextView.setText("");
            mCustomTextView.setId(mIdTextView);
            mContentView.addView(mCustomTextView, MATCH_PARENT, WRAP_CONTENT);
        });
    }

    private void changeTextView(String newTextString) {
        mCustomTextView.setText(newTextString);
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testInitialStates() {
        // Verify the default status for the views are correct
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Button should not be shown after init",
                    mActivity.findViewById(R.id.action_button).isShown());
            Assert.assertFalse("Spinner should not be shown after init",
                    mActivity.findViewById(R.id.progress_spinner).isShown());
        });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testSetStateToButton() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMoreProgressButton.setState(State.BUTTON);

            Assert.assertTrue("Button should be shown with State.BUTTON",
                    mActivity.findViewById(R.id.action_button).isShown());
            Assert.assertFalse("Spinner should not be shown with State.BUTTON",
                    mActivity.findViewById(R.id.progress_spinner).isShown());
        });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testSetStateToSpinner() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMoreProgressButton.setState(State.LOADING);

            Assert.assertFalse("Button should not be shown with State.LOADING",
                    mActivity.findViewById(R.id.action_button).isShown());
            Assert.assertTrue("Spinner should be shown with State.LOADING",
                    mActivity.findViewById(R.id.progress_spinner).isShown());
        });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testSetStateToHidden() {
        // Change state for the button first, then hide it
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMoreProgressButton.setState(State.BUTTON);
            mMoreProgressButton.setState(State.HIDDEN);

            Assert.assertFalse("Button should not be shown with State.HIDDEN",
                    mActivity.findViewById(R.id.action_button).isShown());
            Assert.assertFalse("Spinner should not be shown with State.HIDDEN",
                    mActivity.findViewById(R.id.progress_spinner).isShown());
        });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testStateAfterBindAction() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean buttonShownBefore = mActivity.findViewById(R.id.action_button).isShown();
            boolean spinnerShownBefore = mActivity.findViewById(R.id.action_button).isShown();

            mMoreProgressButton.setOnClickRunnable(() -> changeTextView(""));

            Assert.assertEquals("Button should stays same visibility before/after bind action",
                    buttonShownBefore, mActivity.findViewById(R.id.action_button).isShown());
            Assert.assertEquals("spinner should stays same visibility before/after bind action",
                    spinnerShownBefore, mActivity.findViewById(R.id.progress_spinner).isShown());
        });
    }

    @Test
    @MediumTest
    @Feature({"MoreProgressButton"})
    public void testClickAfterBindAction() {
        final String str = "Some Test String";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String textViewStr =
                    ((TextView) mActivity.findViewById(mIdTextView)).getText().toString();
            Assert.assertNotEquals(str, textViewStr);

            mMoreProgressButton.setOnClickRunnable(() -> changeTextView(str));
            mMoreProgressButton.setState(State.BUTTON);

            Assert.assertTrue(mActivity.findViewById(R.id.action_button).isClickable());

            mActivity.findViewById(R.id.action_button).performClick();

            Assert.assertFalse(mActivity.findViewById(R.id.action_button).isShown());
            Assert.assertTrue(mActivity.findViewById(R.id.progress_spinner).isShown());

            textViewStr = ((TextView) mActivity.findViewById(mIdTextView)).getText().toString();
            Assert.assertEquals(str, textViewStr);
        });
    }
}
