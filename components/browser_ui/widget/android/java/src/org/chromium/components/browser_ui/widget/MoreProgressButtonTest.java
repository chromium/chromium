// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.MoreProgressButton.State;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link MoreProgressButton}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MoreProgressButtonTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static FrameLayout sContentView;

    private MoreProgressButton mMoreProgressButton;
    private TextView mCustomTextView;

    private int mIdTextView;
    private int mIdMoreProgressButton;

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = activityTestRule.getActivity();
                    sContentView = new FrameLayout(sActivity);
                    sActivity.setContentView(sContentView);
                });
    }

    @Before
    public void setupTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();

                    mIdTextView = View.generateViewId();
                    mIdMoreProgressButton = View.generateViewId();

                    mMoreProgressButton =
                            (MoreProgressButton)
                                    LayoutInflater.from(sContentView.getContext())
                                            .inflate(R.layout.more_progress_button, null);
                    mMoreProgressButton.setId(mIdMoreProgressButton);
                    sContentView.addView(mMoreProgressButton, MATCH_PARENT, WRAP_CONTENT);

                    mCustomTextView = new TextView(sActivity);
                    mCustomTextView.setText("");
                    mCustomTextView.setId(mIdTextView);
                    sContentView.addView(mCustomTextView, MATCH_PARENT, WRAP_CONTENT);
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            "Button should not be shown after init",
                            sActivity.findViewById(R.id.action_button).isShown());
                    Assert.assertFalse(
                            "Spinner should not be shown after init",
                            sActivity.findViewById(R.id.progress_spinner).isShown());
                });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testSetStateToButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMoreProgressButton.setState(State.BUTTON);

                    Assert.assertTrue(
                            "Button should be shown with State.BUTTON",
                            sActivity.findViewById(R.id.action_button).isShown());
                    Assert.assertFalse(
                            "Spinner should not be shown with State.BUTTON",
                            sActivity.findViewById(R.id.progress_spinner).isShown());
                });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testSetStateToSpinner() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMoreProgressButton.setState(State.LOADING);

                    Assert.assertFalse(
                            "Button should not be shown with State.LOADING",
                            sActivity.findViewById(R.id.action_button).isShown());
                    Assert.assertTrue(
                            "Spinner should be shown with State.LOADING",
                            sActivity.findViewById(R.id.progress_spinner).isShown());
                });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testSetStateToHidden() {
        // Change state for the button first, then hide it
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMoreProgressButton.setState(State.BUTTON);
                    mMoreProgressButton.setState(State.HIDDEN);

                    Assert.assertFalse(
                            "Button should not be shown with State.HIDDEN",
                            sActivity.findViewById(R.id.action_button).isShown());
                    Assert.assertFalse(
                            "Spinner should not be shown with State.HIDDEN",
                            sActivity.findViewById(R.id.progress_spinner).isShown());
                });
    }

    @Test
    @SmallTest
    @Feature({"MoreProgressButton"})
    public void testStateAfterBindAction() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean buttonShownBefore =
                            sActivity.findViewById(R.id.action_button).isShown();
                    boolean spinnerShownBefore =
                            sActivity.findViewById(R.id.progress_spinner).isShown();

                    mMoreProgressButton.setOnClickRunnable(() -> changeTextView(""));

                    Assert.assertEquals(
                            "Button should stays same visibility before/after bind action",
                            buttonShownBefore,
                            sActivity.findViewById(R.id.action_button).isShown());
                    Assert.assertEquals(
                            "spinner should stays same visibility before/after bind action",
                            spinnerShownBefore,
                            sActivity.findViewById(R.id.progress_spinner).isShown());
                });
    }

    @Test
    @MediumTest
    @Feature({"MoreProgressButton"})
    public void testClickAfterBindAction() {
        final String str = "Some Test String";

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String textViewStr =
                            ((TextView) sActivity.findViewById(mIdTextView)).getText().toString();
                    Assert.assertNotEquals(str, textViewStr);

                    mMoreProgressButton.setOnClickRunnable(() -> changeTextView(str));
                    mMoreProgressButton.setState(State.BUTTON);

                    Assert.assertTrue(sActivity.findViewById(R.id.action_button).isClickable());

                    sActivity.findViewById(R.id.action_button).performClick();

                    Assert.assertFalse(sActivity.findViewById(R.id.action_button).isShown());
                    Assert.assertTrue(sActivity.findViewById(R.id.progress_spinner).isShown());

                    textViewStr =
                            ((TextView) sActivity.findViewById(mIdTextView)).getText().toString();
                    Assert.assertEquals(str, textViewStr);
                });
    }
}
