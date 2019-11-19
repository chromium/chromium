// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link RadioButtonLayout}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class RadioButtonWithDescriptionLayoutTest {
    private static final String NON_ZERO_MARGIN_ASSERT_MESSAGE =
            "First N-1 items should have a non-zero margin";
    private static final String ZERO_MARGIN_ASSERT_MESSAGE =
            "The last item should have a zero margin";
    private static final String TITLE_MATCH_ASSERT_MESSAGE =
            "Title set through addOptions should match the view's title.";
    private static final String DESCRIPTION_MATCH_ASSERT_MESSAGE =
            "Description set through addOptions should match the view's description.";
    private static final String TAG_MATCH_ASSERT_MESSAGE =
            "Tag set through addOptions should match the view's tag.";

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testMargins() {
        RadioButtonWithDescriptionLayout layout = new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of options.
        List<RadioButtonWithDescriptionLayout.Option> options = new ArrayList<>();
        options.add(new RadioButtonWithDescriptionLayout.Option("a", "a_desc", "a_tag"));
        options.add(new RadioButtonWithDescriptionLayout.Option("b", "b_desc", "b_tag"));
        options.add(new RadioButtonWithDescriptionLayout.Option("c", "c_desc", "c_tag"));
        layout.addOptions(options);
        Assert.assertEquals(3, layout.getChildCount());

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();

            if (i < layout.getChildCount() - 1) {
                Assert.assertNotEquals(NON_ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            } else {
                Assert.assertEquals(ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            }
        }

        // Add more options.
        List<RadioButtonWithDescriptionLayout.Option> moreOptions = new ArrayList<>();
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("d", "d_desc", null));
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("e", "e_desc", null));
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("f", "f_desc", null));
        layout.addOptions(moreOptions);
        Assert.assertEquals(6, layout.getChildCount());

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();

            if (i < layout.getChildCount() - 1) {
                Assert.assertNotEquals(NON_ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            } else {
                Assert.assertEquals(ZERO_MARGIN_ASSERT_MESSAGE, 0, params.bottomMargin);
            }
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAddOptions() {
        RadioButtonWithDescriptionLayout layout = new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of options.
        List<RadioButtonWithDescriptionLayout.Option> options = new ArrayList<>();
        options.add(new RadioButtonWithDescriptionLayout.Option("a", "a_desc", "a_tag"));
        options.add(new RadioButtonWithDescriptionLayout.Option("b", "b_desc", "b_tag"));
        options.add(new RadioButtonWithDescriptionLayout.Option("c", "c_desc", "c_tag"));
        layout.addOptions(options);
        Assert.assertEquals(3, layout.getChildCount());

        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(
                    TITLE_MATCH_ASSERT_MESSAGE, options.get(i).getTitle(), b.getTitleText());
            Assert.assertEquals(DESCRIPTION_MATCH_ASSERT_MESSAGE, options.get(i).getDescription(),
                    b.getDescriptionText());
            Assert.assertEquals(TAG_MATCH_ASSERT_MESSAGE, options.get(i).getTag(), b.getTag());
        }

        // Add even more options, but without tags.
        List<RadioButtonWithDescriptionLayout.Option> moreOptions = new ArrayList<>();
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("d", "d_desc", null));
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("e", "e_desc", null));
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("f", "f_desc", null));
        layout.addOptions(moreOptions);
        Assert.assertEquals(6, layout.getChildCount());
        for (int i = 0; i < 3; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(
                    TITLE_MATCH_ASSERT_MESSAGE, options.get(i).getTitle(), b.getTitleText());
            Assert.assertEquals(DESCRIPTION_MATCH_ASSERT_MESSAGE, options.get(i).getDescription(),
                    b.getDescriptionText());
            Assert.assertEquals(TAG_MATCH_ASSERT_MESSAGE, options.get(i).getTag(), b.getTag());
        }
        for (int i = 3; i < 6; i++) {
            RadioButtonWithDescription b = (RadioButtonWithDescription) layout.getChildAt(i);
            Assert.assertEquals(TITLE_MATCH_ASSERT_MESSAGE, moreOptions.get(i - 3).getTitle(),
                    b.getTitleText());
            Assert.assertEquals(DESCRIPTION_MATCH_ASSERT_MESSAGE,
                    moreOptions.get(i - 3).getDescription(), b.getDescriptionText());
            Assert.assertEquals(
                    TAG_MATCH_ASSERT_MESSAGE, moreOptions.get(i - 3).getTag(), b.getTag());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelection() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        // Add one set of options.
        List<RadioButtonWithDescriptionLayout.Option> options = new ArrayList<>();
        options.add(new RadioButtonWithDescriptionLayout.Option("a", "a_desc", null));
        options.add(new RadioButtonWithDescriptionLayout.Option("b", "b_desc", null));
        options.add(new RadioButtonWithDescriptionLayout.Option("c", "c_desc", null));
        layout.addOptions(options);
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
        List<RadioButtonWithDescriptionLayout.Option> moreOptions = new ArrayList<>();
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("d", "d_desc", null));
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("e", "e_desc", null));
        moreOptions.add(new RadioButtonWithDescriptionLayout.Option("f", "f_desc", null));
        layout.addOptions(moreOptions);
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

        List<RadioButtonWithDescriptionLayout.Option> options = new ArrayList<>();
        options.add(new RadioButtonWithDescriptionLayout.Option("a", "a_desc", null));
        options.add(new RadioButtonWithDescriptionLayout.Option("b", "b_desc", null));
        options.add(new RadioButtonWithDescriptionLayout.Option("c", "c_desc", null));
        layout.addOptions(options);

        RadioButtonWithDescription firstButton = (RadioButtonWithDescription) layout.getChildAt(0);
        final TextView accessoryTextView = new TextView(mContext);
        layout.attachAccessoryView(accessoryTextView, firstButton);
        Assert.assertEquals(
                "The accessory view should be right after the position of it's attachment host.",
                accessoryTextView, layout.getChildAt(1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAccessoryViewAddedThenReadded() {
        final RadioButtonWithDescriptionLayout layout =
                new RadioButtonWithDescriptionLayout(mContext);

        List<RadioButtonWithDescriptionLayout.Option> options = new ArrayList<>();
        options.add(new RadioButtonWithDescriptionLayout.Option("a", "a_desc", null));
        options.add(new RadioButtonWithDescriptionLayout.Option("b", "b_desc", null));
        options.add(new RadioButtonWithDescriptionLayout.Option("c", "c_desc", null));
        layout.addOptions(options);

        RadioButtonWithDescription firstButton = (RadioButtonWithDescription) layout.getChildAt(0);
        RadioButtonWithDescription lastButton =
                (RadioButtonWithDescription) layout.getChildAt(layout.getChildCount() - 1);
        final TextView accessoryTextView = new TextView(mContext);
        layout.attachAccessoryView(accessoryTextView, firstButton);
        layout.attachAccessoryView(accessoryTextView, lastButton);
        Assert.assertNotEquals(
                "The accessory view shouldn't be in the first position it was inserted at.",
                accessoryTextView, layout.getChildAt(1));
        Assert.assertEquals("The accessory view should be at the new position it was placed at.",
                accessoryTextView, layout.getChildAt(layout.getChildCount() - 1));
    }
}
