// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ValueUtils;

/** Unit tests for {@link ExtensionActionButtonViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionActionButtonViewBinderTest {
    private static final float FLOAT_EPSILON = 1e-4f;

    private ListMenuButton mButton;
    private PropertyModel mModel;
    private float mDisabledAlpha;

    @Before
    public void setUp() {
        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mButton = new ListMenuButton(activity, null);
        mDisabledAlpha =
                ValueUtils.getFloat(
                        mButton.getResources(), org.chromium.ui.R.dimen.default_disabled_alpha);

        mModel =
                new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                        .with(ExtensionActionButtonProperties.ALPHA, 1.0f)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mButton, ExtensionActionButtonViewBinder::bind);
    }

    @Test
    public void testBindAlpha() {
        assertEquals(1.0f, mButton.getAlpha(), FLOAT_EPSILON);

        mModel.set(ExtensionActionButtonProperties.ALPHA, mDisabledAlpha);
        assertEquals(mDisabledAlpha, mButton.getAlpha(), FLOAT_EPSILON);

        mModel.set(ExtensionActionButtonProperties.ALPHA, 1.0f);
        assertEquals(1.0f, mButton.getAlpha(), FLOAT_EPSILON);
    }
}
