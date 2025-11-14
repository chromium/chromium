// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RippleDrawable;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.test.R;

/** Tests for {@link ContainmentViewStyler}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ContainmentViewStylerTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Context mContext;
    private int mDefaultRadius;
    private int mBackgroundColor;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mContext = mSettingsRule.getActivity();
        mDefaultRadius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_default);
        mBackgroundColor = SemanticColorUtils.getSettingsBackgroundColor(mContext);
    }

    @Test
    @SmallTest
    public void testApplyBackgroundStyle_EmptyStylePreservesBackground() {
        View view = new View(mContext);
        Drawable originalBackground = new ColorDrawable(Color.RED);
        view.setBackground(originalBackground);

        ContainmentViewStyler.applyBackgroundStyle(view, ContainerStyle.EMPTY);

        assertSame(
                "The original background should be preserved when the style is EMPTY.",
                originalBackground,
                view.getBackground());
    }

    @Test
    @SmallTest
    public void testApplyBackgroundStyle_LayersBackgrounds() {
        View view = new View(mContext);
        Drawable originalBackground =
                new RippleDrawable(ColorStateList.valueOf(Color.BLUE), null, null);
        view.setBackground(originalBackground);

        ContainerStyle style =
                new ContainerStyle.Builder()
                        .setTopRadius(mDefaultRadius)
                        .setBottomRadius(mDefaultRadius)
                        .setBackgroundColor(mBackgroundColor)
                        .build();
        ContainmentViewStyler.applyBackgroundStyle(view, style);

        Drawable background = view.getBackground();
        assertTrue(
                "The background should be a LayerDrawable.", background instanceof LayerDrawable);

        LayerDrawable layerDrawable = (LayerDrawable) background;
        assertEquals("There should be two layers.", 2, layerDrawable.getNumberOfLayers());
        assertNotEquals(
                "The first layer should be the new background.",
                originalBackground,
                layerDrawable.getDrawable(0));
        assertSame(
                "The second layer should be the original background.",
                originalBackground,
                layerDrawable.getDrawable(1));
    }
}
