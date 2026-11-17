// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link ExtensionAccessControlButtonViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionAccessControlButtonViewBinderTest {
    private static final float FLOAT_EPSILON = 1e-4f;

    private TextView mRequestAccessButton;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        var container =
                LayoutInflater.from(activity).inflate(R.layout.extensions_toolbar_container, null);
        mRequestAccessButton = container.findViewById(R.id.extensions_request_access_button);

        mModel = new PropertyModel.Builder(ExtensionsToolbarProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mModel, mRequestAccessButton, ExtensionAccessControlButtonViewBinder::bind);
    }

    @Test
    public void testBindRequestAccessButtonTint() {
        ColorStateList focusedTint = ColorStateList.valueOf(Color.BLACK);
        ColorStateList unfocusedTint = ColorStateList.valueOf(Color.GRAY);

        mModel.set(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_TINT, focusedTint);
        assertEquals(focusedTint, mRequestAccessButton.getTextColors());
        assertEquals(1.0f, mRequestAccessButton.getAlpha(), FLOAT_EPSILON);

        mModel.set(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_TINT, unfocusedTint);
        assertEquals(unfocusedTint, mRequestAccessButton.getTextColors());
        assertEquals(1.0f, mRequestAccessButton.getAlpha(), FLOAT_EPSILON);
    }
}
