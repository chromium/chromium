// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link CheckBoxWithDescription}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CheckBoxWithDescriptionTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testCreateAndClick() {
        CheckBoxWithDescription checkbox =
                createCheckBoxWithDescription("checkbox_1", "checkbox_1_desc");
        assertEquals("Primary text should match.", "checkbox_1", checkbox.getPrimaryText());
        assertEquals(
                "Primary text should be visible.",
                View.VISIBLE,
                checkbox.getPrimaryTextView().getVisibility());
        assertEquals(
                "Description text should match.", "checkbox_1_desc", checkbox.getDescriptionText());
        assertEquals(
                "Description text should be visible when it is not empty.",
                View.VISIBLE,
                checkbox.getDescriptionTextView().getVisibility());
        assertFalse("The checkbox should be unchecked.", checkbox.isChecked());

        testClick(checkbox);
    }

    @Test
    public void testCreateWithEmptyDescriptionAndClick() {
        CheckBoxWithDescription checkbox = createCheckBoxWithDescription("checkbox_2", "");
        assertEquals("Primary text should match.", "checkbox_2", checkbox.getPrimaryText());
        assertEquals(
                "Primary text should be visible.",
                View.VISIBLE,
                checkbox.getPrimaryTextView().getVisibility());
        assertEquals("Description text should match.", "", checkbox.getDescriptionText());
        assertEquals(
                "Description text should be invisible when it is empty.",
                View.GONE,
                checkbox.getDescriptionTextView().getVisibility());
        assertFalse("The checkbox should be unchecked.", checkbox.isChecked());

        testClick(checkbox);
    }

    @Test
    public void testCreateWithoutDescriptionAndClick() {
        CheckBoxWithDescription checkbox = createCheckBoxWithDescription("checkbox_3", null);
        assertEquals("Primary text should match.", "checkbox_3", checkbox.getPrimaryText());
        assertEquals(
                "Primary text should be visible.",
                View.VISIBLE,
                checkbox.getPrimaryTextView().getVisibility());
        assertEquals(
                "Description text should be invisible when it is not set.",
                View.GONE,
                checkbox.getDescriptionTextView().getVisibility());
        assertFalse("The checkbox should be unchecked.", checkbox.isChecked());

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
        checkbox.onClick(checkbox);
        assertTrue("The checkbox should be checked after click.", checkbox.isChecked());
        checkbox.onClick(checkbox);
        assertFalse("The checkbox should be unchecked after another click.", checkbox.isChecked());
    }
}
