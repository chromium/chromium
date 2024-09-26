// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.mockito.Mockito.verify;

import android.content.Context;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.test.R;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link RadioButtonLayout}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class RadioButtonWithDescriptionLayoutTest {
    private static final String ZERO_MARGIN_ASSERT_MESSAGE =
            "The last item should have a zero margin";
    private static final String PRIMARY_MATCH_ASSERT_MESSAGE =
            "Primary text set through addButtons should match the view's primary text.";
    private static final String DESCRIPTION_MATCH_ASSERT_MESSAGE =
            "Description set through addButtons should match the view's description.";
    private static final String TAG_MATCH_ASSERT_MESSAGE =
            "Tag set through addButtons should match the view's tag.";

    private static final String PRIMARY_MATCH_FROM_XML_ASSERT_MESSAGE =
            "Primary text set through layout should match the view's primary text.";
    private static final String DESC_MATCH_FROM_XML_ASSERT_MESSAGE =
            "Description text set through layout should match the view's description.";
    private static final String HINT_MATCH_FROM_XML_ASSERT_MESSAGE =
            "Hint message set through layout should match the view's hint message.";

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        InstrumentationRegistry.getTargetContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testMargins() {
        RadioButtonWithDescriptionLayout layout = new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of buttons.
        List<RadioButtonWithDescription> buttons =
                Arrays.asList(
                        createRadioButtonWithDescription("a", "a_desc", "a_tag"),
                        createRadioButtonWithDescription("b", "b_desc", "b_tag"),
                        createRadioButtonWithDescription("c", "c_desc", "c_tag"));
        layout.addButtons(buttons);
        Assert.assertEquals(3, layout.getChildCount());

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();
            Assert.assertEquals(ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
        }

        // Add more buttons.
        List<RadioButtonWithDescription> moreButtons =
                Arrays.asList(
                        createRadioButtonWithDescription("d", "d_desc", null),
                        createRadioButtonWithDescription("e", "e_desc", null),
                        createRadioButtonWithDescription("f", "f_desc", null));
        layout.addButtons(moreButtons);
        Assert.assertEquals(6, layout.getChildCount());

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();
            Assert.assertEquals(ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAddButtons() {
        RadioButtonWithDescriptionLayout layout = new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of options.
        List<RadioButtonWithDescription> buttons =
                Arrays.asList(
                        createRadioButtonWithDescription("a", "a_desc", "a_tag"),
                        createRadioButtonWithDescription("b", "b_desc", "b_tag"),
                        createRadioButtonWithDescription("c", "c_desc", "c_tag"));
        layout.addButtons(buttons);
        Assert.assertEquals(3, layout.getChildCount());

        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(
                    PRIMARY_MATCH_ASSERT_MESSAGE,
                    buttons.get(i).getPrimaryText().toString(),
                    b.getPrimaryText().toString());
            Assert.assertEquals(
                    DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    buttons.get(i).getDescriptionText().toString(),
                    b.getDescriptionText().toString());
            Assert.assertEquals(TAG_MATCH_ASSERT_MESSAGE, buttons.get(i).getTag(), b.getTag());
        }

        // Add even more options, but without tags.
        List<RadioButtonWithDescription> moreButtons =
                Arrays.asList(
                        createRadioButtonWithDescription("d", "d_desc", null),
                        createRadioButtonWithDescription("e", "e_desc", null),
                        createRadioButtonWithDescription("f", "f_desc", null));
        layout.addButtons(moreButtons);
        Assert.assertEquals(6, layout.getChildCount());
        for (int i = 0; i < 3; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(
                    PRIMARY_MATCH_ASSERT_MESSAGE,
                    buttons.get(i).getPrimaryText().toString(),
                    b.getPrimaryText().toString());
            Assert.assertEquals(
                    DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    buttons.get(i).getDescriptionText().toString(),
                    b.getDescriptionText().toString());
            Assert.assertEquals(TAG_MATCH_ASSERT_MESSAGE, buttons.get(i).getTag(), b.getTag());
        }
        for (int i = 3; i < 6; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(
                    PRIMARY_MATCH_ASSERT_MESSAGE,
                    moreButtons.get(i - 3).getPrimaryText().toString(),
                    b.getPrimaryText().toString());
            Assert.assertEquals(
                    DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    moreButtons.get(i - 3).getDescriptionText().toString(),
                    b.getDescriptionText().toString());
            Assert.assertEquals(
                    TAG_MATCH_ASSERT_MESSAGE, moreButtons.get(i - 3).getTag(), b.getTag());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelection() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of options.
        List<RadioButtonWithDescription> buttons =
                Arrays.asList(
                        createRadioButtonWithDescription("a", "a_desc", null),
                        createRadioButtonWithDescription("b", "b_desc", null),
                        createRadioButtonWithDescription("c", "c_desc", null));
        layout.addButtons(buttons);
        Assert.assertEquals(3, layout.getChildCount());

        // Nothing should be selected by default.
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription child = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertFalse(child.isChecked());
        }

        // Select the second one.
        layout.selectChildAtIndexForTesting(1);
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription child = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(i == 1, child.isChecked());
        }

        // Add even more options.
        List<RadioButtonWithDescription> moreButtons =
                Arrays.asList(
                        createRadioButtonWithDescription("d", "d_desc", null),
                        createRadioButtonWithDescription("e", "e_desc", null),
                        createRadioButtonWithDescription("f", "f_desc", null));
        layout.addButtons(moreButtons);
        Assert.assertEquals(6, layout.getChildCount());

        // Second child should still be checked.
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription child = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(i == 1, child.isChecked());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAccessoryViewAdded() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        List<RadioButtonWithDescription> buttons =
                Arrays.asList(
                        createRadioButtonWithDescription("a", "a_desc", null),
                        createRadioButtonWithDescription("b", "b_desc", null),
                        createRadioButtonWithDescription("c", "c_desc", null));
        layout.addButtons(buttons);

        RadioButtonWithDescription firstButton = (RadioButtonWithDescription) layout.getChildAt(0);
        final TextView accessoryTextView = new TextView(mContext);
        layout.attachAccessoryView(accessoryTextView, firstButton);
        Assert.assertEquals(
                "The accessory view should be right after the position of it's attachment host.",
                accessoryTextView,
                layout.getChildAt(1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAccessoryViewAddedThenReadded() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        List<RadioButtonWithDescription> buttons =
                Arrays.asList(
                        createRadioButtonWithDescription("a", "a_desc", null),
                        createRadioButtonWithDescription("b", "b_desc", null),
                        createRadioButtonWithDescription("c", "c_desc", null));
        layout.addButtons(buttons);

        RadioButtonWithDescription firstButton = (RadioButtonWithDescription) layout.getChildAt(0);
        RadioButtonWithDescription lastButton =
                (RadioButtonWithDescription) layout.getChildAt(layout.getChildCount() - 1);
        final TextView accessoryTextView = new TextView(mContext);
        layout.attachAccessoryView(accessoryTextView, firstButton);
        layout.attachAccessoryView(accessoryTextView, lastButton);
        Assert.assertNotEquals(
                "The accessory view shouldn't be in the first position it was inserted at.",
                accessoryTextView,
                layout.getChildAt(1));
        Assert.assertEquals(
                "The accessory view should be at the new position it was placed at.",
                accessoryTextView,
                layout.getChildAt(layout.getChildCount() - 1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCombinedRadioButtons() {
        // Test if radio buttons are set up correctly when there are multiple classes in the same
        // layout.
        View content =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.radio_button_with_description_layout_test, null, false);

        RadioButtonWithDescriptionLayout layout =
                content.findViewById(R.id.test_radio_button_layout);
        RadioButtonWithDescription b1 = content.findViewById(R.id.test_radio_description_1);
        RadioButtonWithDescription b2 = content.findViewById(R.id.test_radio_description_2);
        RadioButtonWithEditText b3 = content.findViewById(R.id.test_radio_edit_text_1);
        RadioButtonWithEditText b4 = content.findViewById(R.id.test_radio_edit_text_2);
        RadioButtonWithDescriptionAndAuxButton b5 =
                content.findViewById(R.id.test_radio_description_and_aux_button_1);

        Assert.assertNotNull(layout);
        Assert.assertNotNull(b1);
        Assert.assertNotNull(b2);
        Assert.assertNotNull(b3);
        Assert.assertNotNull(b4);
        Assert.assertNotNull(b5);

        layout.selectChildAtIndexForTesting(3);

        Assert.assertEquals(
                PRIMARY_MATCH_FROM_XML_ASSERT_MESSAGE,
                b1.getPrimaryText().toString(),
                mContext.getResources().getString(R.string.test_primary_1));
        Assert.assertEquals(
                PRIMARY_MATCH_FROM_XML_ASSERT_MESSAGE,
                b2.getPrimaryText().toString(),
                mContext.getResources().getString(R.string.test_primary_2));
        Assert.assertEquals(
                PRIMARY_MATCH_FROM_XML_ASSERT_MESSAGE,
                b3.getPrimaryText().toString(),
                mContext.getResources().getString(R.string.test_primary_3));
        Assert.assertEquals(
                PRIMARY_MATCH_FROM_XML_ASSERT_MESSAGE,
                b4.getPrimaryText().toString(),
                mContext.getResources().getString(R.string.test_primary_4));
        Assert.assertEquals(
                PRIMARY_MATCH_FROM_XML_ASSERT_MESSAGE,
                b5.getPrimaryText().toString(),
                mContext.getResources().getString(R.string.test_primary_5));

        Assert.assertTrue(
                DESC_MATCH_FROM_XML_ASSERT_MESSAGE, TextUtils.isEmpty(b1.getDescriptionText()));
        Assert.assertEquals(
                DESC_MATCH_FROM_XML_ASSERT_MESSAGE,
                b2.getDescriptionText().toString(),
                mContext.getResources().getString(R.string.test_desc_2));
        Assert.assertTrue(
                DESC_MATCH_FROM_XML_ASSERT_MESSAGE, TextUtils.isEmpty(b3.getDescriptionText()));
        Assert.assertEquals(
                DESC_MATCH_FROM_XML_ASSERT_MESSAGE,
                b4.getDescriptionText().toString(),
                mContext.getResources().getString(R.string.test_desc_4));
        Assert.assertEquals(
                DESC_MATCH_FROM_XML_ASSERT_MESSAGE,
                b5.getDescriptionText().toString(),
                mContext.getResources().getString(R.string.test_desc_5));

        Assert.assertEquals(
                HINT_MATCH_FROM_XML_ASSERT_MESSAGE,
                ((EditText) b3.getPrimaryTextView()).getHint().toString(),
                mContext.getResources().getString(R.string.test_uri));
        Assert.assertEquals(
                HINT_MATCH_FROM_XML_ASSERT_MESSAGE,
                ((EditText) b4.getPrimaryTextView()).getHint().toString(),
                mContext.getResources().getString(R.string.test_uri));

        Assert.assertFalse(b1.isChecked());
        Assert.assertFalse(b2.isChecked());
        Assert.assertFalse(b3.isChecked());
        Assert.assertTrue(b4.isChecked());
        Assert.assertFalse(b5.isChecked());
    }

    @Test
    @SmallTest
    public void testSetEnable() {
        View content =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.radio_button_with_description_layout_test, null, false);

        RadioButtonWithDescriptionLayout layout =
                content.findViewById(R.id.test_radio_button_layout);
        RadioButtonWithDescription b1 = content.findViewById(R.id.test_radio_description_1);
        RadioButtonWithDescription b2 = content.findViewById(R.id.test_radio_description_2);
        RadioButtonWithEditText b3 = content.findViewById(R.id.test_radio_edit_text_1);
        RadioButtonWithEditText b4 = content.findViewById(R.id.test_radio_edit_text_2);

        final TextView textView1 = new TextView(mContext);
        final TextView textView3 = new TextView(mContext);

        layout.attachAccessoryView(textView1, b1);
        layout.attachAccessoryView(textView3, b3);

        layout.setEnabled(false);

        Assert.assertFalse(b1.isEnabled());
        Assert.assertFalse(b2.isEnabled());
        Assert.assertFalse(b3.isEnabled());
        Assert.assertFalse(b4.isEnabled());
        Assert.assertFalse(textView1.isEnabled());
        Assert.assertFalse(textView3.isEnabled());

        layout.setEnabled(true);

        Assert.assertTrue(b1.isEnabled());
        Assert.assertTrue(b2.isEnabled());
        Assert.assertTrue(b3.isEnabled());
        Assert.assertTrue(b4.isEnabled());
        Assert.assertTrue(textView1.isEnabled());
        Assert.assertTrue(textView3.isEnabled());
    }

    @Test
    @SmallTest
    public void testOnButtonCheckedStateChanged() {
        View content =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.radio_button_with_description_layout_test, null, false);
        RadioButtonWithDescriptionLayout layout =
                content.findViewById(R.id.test_radio_button_layout);
        RadioButtonWithDescription b1 = content.findViewById(R.id.test_radio_description_1);

        RadioGroup.OnCheckedChangeListener listener =
                Mockito.mock(RadioGroup.OnCheckedChangeListener.class);
        layout.setOnCheckedChangeListener(listener);

        layout.onButtonCheckedStateChanged(b1);
        verify(listener).onCheckedChanged(layout, b1.getId());
    }

    @Test
    @SmallTest
    public void testOnButtonCheckedStateChanged_nullObserver() {
        View content =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.radio_button_with_description_layout_test, null, false);
        RadioButtonWithDescriptionLayout layout =
                content.findViewById(R.id.test_radio_button_layout);
        RadioButtonWithDescription b1 = content.findViewById(R.id.test_radio_description_1);

        layout.setOnCheckedChangeListener(null);

        try {
            layout.onButtonCheckedStateChanged(b1);
        } catch (NullPointerException e) {
            throw new AssertionError("No exception should be thrown when the observer is null", e);
        }
    }

    private RadioButtonWithDescription createRadioButtonWithDescription(
            String primary, String description, Object tag) {
        RadioButtonWithDescription b = new RadioButtonWithDescription(mContext, null);
        b.setPrimaryText(primary);
        b.setDescriptionText(description);
        b.setTag(tag);
        return b;
    }
}
