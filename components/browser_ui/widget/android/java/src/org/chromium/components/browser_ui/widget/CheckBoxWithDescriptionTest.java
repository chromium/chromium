// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.mockito.Mockito.mock;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.test.R;

/** Tests for {@link CheckBoxWithDescription}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CheckBoxWithDescriptionTest {
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
    public void testCreateAndClick() {
        CheckBoxWithDescription checkbox =
                createCheckBoxWithDescription("checkbox_1", "checkbox_1_desc");
        Assert.assertEquals("Primary text should match.", "checkbox_1", checkbox.getPrimaryText());
        Assert.assertEquals(
                "Primary text should be visible.",
                View.VISIBLE,
                checkbox.getPrimaryTextView().getVisibility());
        Assert.assertEquals(
                "Description text should match.", "checkbox_1_desc", checkbox.getDescriptionText());
        Assert.assertEquals(
                "Description text should be visible when it is not empty.",
                View.VISIBLE,
                checkbox.getDescriptionTextView().getVisibility());
        Assert.assertFalse("The checkbox should be unchecked.", checkbox.isChecked());

        testClick(checkbox);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCreateWithEmptyDescriptionAndClick() {
        CheckBoxWithDescription checkbox = createCheckBoxWithDescription("checkbox_2", "");
        Assert.assertEquals("Primary text should match.", "checkbox_2", checkbox.getPrimaryText());
        Assert.assertEquals(
                "Primary text should be visible.",
                View.VISIBLE,
                checkbox.getPrimaryTextView().getVisibility());
        Assert.assertEquals("Description text should match.", "", checkbox.getDescriptionText());
        Assert.assertEquals(
                "Description text should be invisible when it is empty.",
                View.GONE,
                checkbox.getDescriptionTextView().getVisibility());
        Assert.assertFalse("The checkbox should be unchecked.", checkbox.isChecked());

        testClick(checkbox);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCreateWithoutDescriptionAndClick() {
        CheckBoxWithDescription checkbox = createCheckBoxWithDescription("checkbox_3", null);
        Assert.assertEquals("Primary text should match.", "checkbox_3", checkbox.getPrimaryText());
        Assert.assertEquals(
                "Primary text should be visible.",
                View.VISIBLE,
                checkbox.getPrimaryTextView().getVisibility());
        Assert.assertEquals(
                "Description text should be invisible when it is not set.",
                View.GONE,
                checkbox.getDescriptionTextView().getVisibility());
        Assert.assertFalse("The checkbox should be unchecked.", checkbox.isChecked());

        testClick(checkbox);
    }

    private CheckBoxWithDescription createCheckBoxWithDescription(
            String primary, @Nullable String description) {
        CheckBoxWithDescription checkbox = new CheckBoxWithDescription(mContext, null);
        checkbox.setPrimaryText(primary);
        if (description != null) {
            checkbox.setDescriptionText(description);
        }
        return checkbox;
    }

    private void testClick(CheckBoxWithDescription checkbox) {
        View view = mock(View.class);
        checkbox.onClick(view);
        Assert.assertTrue("The checkbox should be checked after click.", checkbox.isChecked());
        checkbox.onClick(view);
        Assert.assertFalse(
                "The checkbox should be unchecked after another click.", checkbox.isChecked());
    }
}
