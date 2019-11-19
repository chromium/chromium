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
import android.widget.RadioButton;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link RadioButtonLayout}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class RadioButtonLayoutTest {
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
        RadioButtonLayout layout = new RadioButtonLayout(mContext);

        // Add one set of options.
        List<CharSequence> messages = new ArrayList<CharSequence>();
        messages.add("a");
        messages.add("b");
        messages.add("c");
        layout.addOptions(messages, null);

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();

            if (i < layout.getChildCount() - 1) {
                Assert.assertNotEquals(0, params.bottomMargin);
            } else {
                Assert.assertEquals(0, params.bottomMargin);
            }
        }

        // Add more options.
        List<CharSequence> moreMessages = new ArrayList<CharSequence>();
        moreMessages.add("d");
        moreMessages.add("e");
        moreMessages.add("f");
        layout.addOptions(moreMessages, null);

        // Test the margins.
        for (int i = 0; i < layout.getChildCount(); i++) {
            View child = layout.getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();

            if (i < layout.getChildCount() - 1) {
                Assert.assertNotEquals(0, params.bottomMargin);
            } else {
                Assert.assertEquals(0, params.bottomMargin);
            }
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAddOptions() {
        RadioButtonLayout layout = new RadioButtonLayout(mContext);

        // Add one set of options.
        List<CharSequence> messages = new ArrayList<CharSequence>();
        messages.add("a");
        messages.add("b");
        messages.add("c");
        List<String> tags = Arrays.asList("tag 1", "tag 2", "tag 3");
        layout.addOptions(messages, tags);
        Assert.assertEquals(3, layout.getChildCount());
        for (int i = 0; i < layout.getChildCount(); i++) {
            Assert.assertEquals(messages.get(i), ((RadioButton) layout.getChildAt(i)).getText());
            Assert.assertEquals(tags.get(i), ((RadioButton) layout.getChildAt(i)).getTag());
        }

        // Add even more options, but without tags.
        List<CharSequence> moreMessages = new ArrayList<CharSequence>();
        moreMessages.add("d");
        moreMessages.add("e");
        moreMessages.add("f");
        layout.addOptions(moreMessages, null);
        Assert.assertEquals(6, layout.getChildCount());
        for (int i = 0; i < 3; i++) {
            Assert.assertEquals(messages.get(i), ((RadioButton) layout.getChildAt(i)).getText());
            Assert.assertEquals(tags.get(i), ((RadioButton) layout.getChildAt(i)).getTag());
        }
        for (int i = 3; i < 6; i++) {
            Assert.assertEquals(
                    moreMessages.get(i - 3), ((RadioButton) layout.getChildAt(i)).getText());
            Assert.assertNull(((RadioButton) layout.getChildAt(i)).getTag());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelection() {
        final RadioButtonLayout layout = new RadioButtonLayout(mContext);

        // Add one set of options.
        List<CharSequence> messages = new ArrayList<CharSequence>();
        messages.add("a");
        messages.add("b");
        messages.add("c");
        layout.addOptions(messages, null);
        Assert.assertEquals(3, layout.getChildCount());

        // Nothing should be selected by default.
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButton child = (RadioButton) layout.getChildAt(i);
            Assert.assertFalse(child.isChecked());
        }

        // Select the second one.
        layout.selectChildAtIndex(1);
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButton child = (RadioButton) layout.getChildAt(i);
            Assert.assertEquals(i == 1, child.isChecked());
        }

        // Add even more options.
        List<CharSequence> moreMessages = new ArrayList<CharSequence>();
        moreMessages.add("d");
        moreMessages.add("e");
        moreMessages.add("f");
        layout.addOptions(moreMessages, null);
        Assert.assertEquals(6, layout.getChildCount());

        // Second child should still be checked.
        for (int i = 0; i < layout.getChildCount(); i++) {
            RadioButton child = (RadioButton) layout.getChildAt(i);
            Assert.assertEquals(i == 1, child.isChecked());
        }
    }
}
