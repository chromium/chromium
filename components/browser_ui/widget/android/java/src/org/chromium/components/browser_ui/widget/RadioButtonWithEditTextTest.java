// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.text.InputType;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
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
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for {@link RadioButtonWithEditText}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class RadioButtonWithEditTextTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static class TestListener implements RadioButtonWithEditText.OnTextChangeListener {
        private CharSequence mCurrentText;
        private int mNumberOfTimesTextChanged;

        TestListener() {
            mNumberOfTimesTextChanged = 0;
        }

        /** Will be called when the text edit has a value change. */
        @Override
        public void onTextChanged(CharSequence newText) {
            mCurrentText = newText;
            mNumberOfTimesTextChanged += 1;
        }

        void setCurrentText(CharSequence currentText) {
            mCurrentText = currentText;
        }

        /**
         * Get the current text stored inside
         *
         * @return current text updated by RadioButtonWithEditText
         */
        CharSequence getCurrentText() {
            return mCurrentText;
        }

        int getTimesCalled() {
            return mNumberOfTimesTextChanged;
        }
    }

    private static Activity sActivity;
    private static FrameLayout sContentView;

    private TestListener mListener;
    private RadioButtonWithEditText mRadioButtonWithEditText;
    private RadioButton mButton;
    private EditText mEditText;
    private Button mDummyButton;

    @BeforeClass
    public static void setupSuite() {
        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);
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
        mListener = new TestListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();
                    View layout =
                            LayoutInflater.from(sActivity)
                                    .inflate(
                                            R.layout.radio_button_with_edit_text_test, null, false);
                    sContentView.addView(layout, MATCH_PARENT, WRAP_CONTENT);

                    mRadioButtonWithEditText =
                            (RadioButtonWithEditText) layout.findViewById(R.id.test_radio_button);
                    mDummyButton = (Button) layout.findViewById(R.id.dummy_button);
                    Assert.assertNotNull(mRadioButtonWithEditText);
                    Assert.assertNotNull(mDummyButton);

                    mButton = layout.findViewById(R.id.radio_button);
                    mEditText = layout.findViewById(R.id.edit_text);

                    Assert.assertNotNull("Radio Button should not be null", mButton);
                    Assert.assertNotNull("Edit Text should not be null", mEditText);
                });
    }

    @Test
    @SmallTest
    public void testViewSetup() {
        Assert.assertFalse("Button should not be set checked after init.", mButton.isChecked());
        Assert.assertTrue(
                "Text entry should be empty after init.", TextUtils.isEmpty(mEditText.getText()));

        // Test if apply attr works
        int textUriInputType = InputType.TYPE_TEXT_VARIATION_URI | InputType.TYPE_CLASS_TEXT;
        Assert.assertEquals(
                "EditText input type is different than attr setting.",
                textUriInputType,
                mEditText.getInputType());
        Assert.assertEquals(
                "EditText input hint is different than attr setting.",
                sActivity.getResources().getString(R.string.test_uri),
                mEditText.getHint());

        TextView description = sActivity.findViewById(R.id.description);
        Assert.assertNotNull("Description should not be null", description);
        Assert.assertEquals(
                "Description is different than attr setting.",
                sActivity.getResources().getString(R.string.test_string),
                description.getText());
    }

    @Test
    @SmallTest
    public void testSetHint() {
        final CharSequence hintMsg = "Text hint";
        final String resourceString = sActivity.getResources().getString(R.string.test_string);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRadioButtonWithEditText.setHint(hintMsg);
                    Assert.assertEquals(
                            "Hint message set from string is different from test setting",
                            hintMsg.toString(),
                            mEditText.getHint().toString());

                    mRadioButtonWithEditText.setHint(R.string.test_string);
                    Assert.assertEquals(
                            "Hint message set from resource id is different from test setting",
                            resourceString,
                            mEditText.getHint().toString());
                });
    }

    @Test
    @SmallTest
    public void testSetInputType() {
        int[] commonInputTypes = {
            InputType.TYPE_CLASS_DATETIME,
            InputType.TYPE_CLASS_NUMBER,
            InputType.TYPE_CLASS_PHONE,
            InputType.TYPE_CLASS_TEXT,
            InputType.TYPE_TEXT_VARIATION_URI,
            InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS,
            InputType.TYPE_DATETIME_VARIATION_DATE,
        };

        for (int type : commonInputTypes) {
            mRadioButtonWithEditText.setInputType(type);
            Assert.assertEquals(mEditText.getInputType(), type);
        }
    }

    @Test
    @SmallTest
    public void testChangeEditText() {
        final CharSequence str1 = "First string";
        final CharSequence str2 = "SeConD sTrINg";

        CharSequence origStr = mRadioButtonWithEditText.getPrimaryText();

        // Test if changing the text edit will result in changing of listener
        mRadioButtonWithEditText.addTextChangeListener(mListener);
        mListener.setCurrentText(origStr);
        int timesCalled = mListener.getTimesCalled();

        // Test changes for edit text
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRadioButtonWithEditText.setPrimaryText(str1);
                });

        Assert.assertEquals(
                "New String value should be updated",
                str1.toString(),
                mRadioButtonWithEditText.getPrimaryText().toString());
        Assert.assertEquals(
                "Text message in listener should be updated accordingly",
                str1.toString(),
                mListener.getCurrentText().toString());
        Assert.assertEquals(
                "TestListener#OnTextChanged should be called once",
                timesCalled + 1,
                mListener.getTimesCalled());

        // change to another text from View
        timesCalled = mListener.getTimesCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEditText.setText(str2);
                });

        Assert.assertEquals(
                "New String value should be updated",
                str2.toString(),
                mRadioButtonWithEditText.getPrimaryText().toString());
        Assert.assertEquals(
                "Text message in listener should be updated accordingly",
                str2.toString(),
                mListener.getCurrentText().toString());
        Assert.assertEquals(
                "TestListener#OnTextChanged should be called once",
                timesCalled + 1,
                mListener.getTimesCalled());

        // change to another text from View
        mRadioButtonWithEditText.removeTextChangeListener(mListener);
        timesCalled = mListener.getTimesCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRadioButtonWithEditText.setPrimaryText(str1);
                });

        Assert.assertEquals(
                "New String value should be updated",
                str1.toString(),
                mRadioButtonWithEditText.getPrimaryText().toString());
        Assert.assertEquals(
                "Text message in listener should not be updated.",
                str2.toString(),
                mListener.getCurrentText().toString());
        Assert.assertEquals(
                "TestListener#OnTextChanged should not be called any more",
                timesCalled,
                mListener.getTimesCalled());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Test is flaky: https://crbug.com/1344713")
    public void testFocusChange() {
        Assert.assertFalse(mRadioButtonWithEditText.hasFocus());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRadioButtonWithEditText.setChecked(true);
                });
        Assert.assertFalse(
                "Edit text should not gain focus when radio button is checked.",
                mEditText.hasFocus());
        waitForCursorVisibility(false);

        // Test requesting focus on the EditText.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEditText.requestFocus();
                });
        waitForCursorVisibility(true);

        // Requesting focus elsewhere.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDummyButton.requestFocus();
                });
        waitForCursorVisibility(false);
        waitForKeyboardVisibility(false);

        // Uncheck the radio button, then click EditText to show keyboard and checked the radio
        // button.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRadioButtonWithEditText.setChecked(false);

                    // Request focus on EditText and show keyboard as if it is clicked.
                    // See https://crbug.com/1177183.
                    mEditText.requestFocus();
                    KeyboardVisibilityDelegate.getInstance().showKeyboard(mEditText);
                });
        waitForCursorVisibility(true);
        waitForKeyboardVisibility(true);
        Assert.assertTrue(
                "RadioButton should be checked after EditText gains focus.",
                mRadioButtonWithEditText.isChecked());

        // Test editor action.
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);
        waitForCursorVisibility(false);
        waitForKeyboardVisibility(false);
    }

    @Test
    @SmallTest
    public void testSetEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRadioButtonWithEditText.setEnabled(false);
                });
        Assert.assertFalse(
                "Primary TextView should be set to disabled.",
                mRadioButtonWithEditText.getPrimaryTextView().isEnabled());
        Assert.assertFalse(
                "Description TextView should be set to disabled.",
                mRadioButtonWithEditText.getDescriptionTextView().isEnabled());
        Assert.assertFalse(
                "RadioButton should be set to disabled.",
                mRadioButtonWithEditText.getRadioButtonView().isEnabled());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRadioButtonWithEditText.setEnabled(true);
                });
        Assert.assertTrue(
                "Primary TextView should be set to enabled.",
                mRadioButtonWithEditText.getPrimaryTextView().isEnabled());
        Assert.assertTrue(
                "Description TextView should be set to enabled.",
                mRadioButtonWithEditText.getDescriptionTextView().isEnabled());
        Assert.assertTrue(
                "RadioButton should be set to enabled.",
                mRadioButtonWithEditText.getRadioButtonView().isEnabled());
    }

    private void waitForKeyboardVisibility(boolean isVisible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Keyboard visibility does not consist with test setting.",
                            KeyboardVisibilityDelegate.getInstance()
                                    .isKeyboardShowing(sActivity, mEditText),
                            Matchers.is(isVisible));
                });
    }

    private void waitForCursorVisibility(boolean isVisible) {
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "Cursor visibility is different.",
                                mEditText.isCursorVisible(),
                                Matchers.is(isVisible)));
    }
}
