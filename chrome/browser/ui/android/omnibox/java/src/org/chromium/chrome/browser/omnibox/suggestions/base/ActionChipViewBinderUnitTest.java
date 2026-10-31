// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RippleDrawable;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link ActionChipViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipViewBinderUnitTest {
    private Context mContext;
    private OmniboxResourceProvider mResourceProvider;
    private ChipView mChip;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mResourceProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        mChip = new ChipView(mContext, R.style.OmniboxActionChipThemeOverlay);
    }

    @Test
    public void resourceProvider_appliesDefaultBackgroundFill() {
        bindChip();
        assertEquals(mResourceProvider.getActionChipBackgroundTintList(), backgroundFillOf(mChip));
    }

    @Test
    public void resourceProvider_appliesIncognitoBackgroundFill() {
        mResourceProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        ColorStateList incognitoFill = mResourceProvider.getActionChipBackgroundTintList();
        assertNotEquals(mContext.getColorStateList(R.color.chip_background_color), incognitoFill);

        bindChip();
        assertEquals(incognitoFill, backgroundFillOf(mChip));
    }

    @Test
    public void chipProperties_delegateToChipViewBinder() {
        PropertyModel model = bindChip();
        assertTrue(mChip.isEnabled());
        assertEquals("Call", mChip.getPrimaryTextView().getText().toString());

        model.set(ChipProperties.ENABLED, false);
        assertFalse(mChip.isEnabled());
        // Chip-level property changes leave the theme intact.
        assertEquals(mResourceProvider.getActionChipBackgroundTintList(), backgroundFillOf(mChip));
    }

    private PropertyModel bindChip() {
        PropertyModel model =
                new PropertyModel.Builder(ActionChipProperties.ALL_KEYS)
                        .with(ActionChipProperties.RESOURCE_PROVIDER, mResourceProvider)
                        .with(ChipProperties.TEXT, "Call")
                        .with(ChipProperties.CONTENT_DESCRIPTION, "Call")
                        .with(ChipProperties.ENABLED, true)
                        .with(
                                ChipProperties.PRIMARY_TEXT_APPEARANCE,
                                R.style.TextAppearance_ChipText)
                        .build();
        PropertyModelChangeProcessor.create(model, mChip, ActionChipViewBinder::bind);
        return model;
    }

    /**
     * Returns the fill color of the chip's background. ChipView routes {@code
     * setBackgroundTintList} to the fill of its background GradientDrawable (not to a View tint
     * filter), which RippleBackgroundHelper nests as Ripple > [Inset >] Layer[0].
     */
    private static ColorStateList backgroundFillOf(ChipView chip) {
        Drawable d = ((RippleDrawable) chip.getBackground()).getDrawable(0);
        if (d instanceof InsetDrawable inset) d = inset.getDrawable();
        return ((GradientDrawable) ((LayerDrawable) d).getDrawable(0)).getColor();
    }
}
