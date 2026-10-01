// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link NumberRollView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NumberRollViewTest {
    private NumberRollView mNumberRollView;
    private TextView mDownNumber;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mNumberRollView =
                (NumberRollView)
                        LayoutInflater.from(context).inflate(R.layout.number_roll_view, null);
        mDownNumber = mNumberRollView.findViewById(R.id.down);

        mNumberRollView.setString(R.plurals.selected_items);
        mNumberRollView.setStringForZero(R.string.select_items);
        mNumberRollView.setNumber(0, false);
    }

    @Test
    public void testStringForZero() {
        assertEquals("Select items", mDownNumber.getText());

        mNumberRollView.setStringForZero("Test title");
        assertEquals("Test title", mDownNumber.getText());

        mNumberRollView.setNumber(1, false);
        mNumberRollView.setStringForZero("Test title 2");
        assertEquals("1 selected", mDownNumber.getText());

        mNumberRollView.setNumber(0, false);
        assertEquals("Test title 2", mDownNumber.getText());
    }
}
