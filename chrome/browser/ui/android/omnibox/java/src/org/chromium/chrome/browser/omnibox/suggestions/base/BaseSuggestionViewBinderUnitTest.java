// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link BaseSuggestionViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseSuggestionViewBinderUnitTest {

    private Context mBareContext;
    private Context mContext;
    private Resources mResources;
    private PropertyModel mModel;
    private BaseSuggestionView<View> mBaseView;
    private ImageView mIconView;

    @Before
    public void setUp() {
        // This context has no themes, no resources, no images, no backgrounds etc.
        mBareContext = ContextUtils.getApplicationContext();
        mContext = new ContextThemeWrapper(mBareContext, R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();

        mBaseView = spy(new BaseSuggestionView(new ImageView(mContext)));
        mIconView = mBaseView.decorationIcon;

        mModel = new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel,
                mBaseView,
                new BaseSuggestionViewBinder(
                        (m, v, p) -> {
                            assertEquals(mBaseView.contentView, v);
                        }));
        BaseSuggestionViewBinder.initializeDimensions(mContext);
    }

    @Test
    public void decorIcon_showSquareIcon() {
        OmniboxDrawableState state =
                new OmniboxDrawableState(
                        new ColorDrawable(0),
                        /* useRoundedCorners= */ false,
                        /* isLarge= */ false,
                        /* allowTint= */ false);
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        assertFalse(mIconView.getClipToOutline());
        assertEquals(View.VISIBLE, mIconView.getVisibility());
        assertEquals(state.drawable, mIconView.getDrawable());
    }

    @Test
    public void decorIcon_showRoundedIcon() {
        OmniboxDrawableState state = OmniboxDrawableState.forColor(0);
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        assertTrue(mIconView.getClipToOutline());
        assertEquals(View.VISIBLE, mIconView.getVisibility());
        assertEquals(state.drawable, mIconView.getDrawable());
    }

    @Test
    public void decorIcon_hideIcon() {
        OmniboxDrawableState state = OmniboxDrawableState.forColor(0);
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        assertEquals(View.VISIBLE, mIconView.getVisibility());
        assertEquals(state.drawable, mIconView.getDrawable());

        mModel.set(BaseSuggestionViewProperties.ICON, null);
        assertEquals(View.GONE, mIconView.getVisibility());
        // Ensure we're releasing drawable to free memory.
        assertNull(mIconView.getDrawable());
    }

    @Test
    public void actionIcon_showIcon() {
        Runnable callback = mock(Runnable.class);
        List<Action> list =
                Arrays.asList(
                        new Action(
                                mContext,
                                OmniboxDrawableState.forColor(0),
                                R.string.accessibility_omnibox_btn_refine,
                                callback));
        mModel.set(BaseSuggestionViewProperties.ACTION_BUTTONS, list);

        List<ImageView> actionButtons = mBaseView.getActionButtons();
        Assert.assertEquals(1, actionButtons.size());
        Assert.assertEquals(View.VISIBLE, actionButtons.get(0).getVisibility());
        Assert.assertEquals(list.get(0).icon.drawable, actionButtons.get(0).getDrawable());
        Assert.assertNotNull(actionButtons.get(0).getBackground());
        verify(mBaseView, times(1)).addView(actionButtons.get(0));

        Assert.assertTrue(actionButtons.get(0).performClick());
        Assert.assertTrue(actionButtons.get(0).performClick());
        Assert.assertTrue(actionButtons.get(0).performClick());
        verify(callback, times(3)).run();
    }

    @Test
    public void actionIcon_showMultipleIcons() {
        Runnable call1 = mock(Runnable.class);
        Runnable call2 = mock(Runnable.class);
        Runnable call3 = mock(Runnable.class);

        List<Action> list =
                Arrays.asList(
                        new Action(
                                mContext,
                                OmniboxDrawableState.forColor(0),
                                R.string.accessibility_omnibox_btn_refine,
                                call1),
                        new Action(
                                mContext,
                                OmniboxDrawableState.forColor(0),
                                R.string.accessibility_omnibox_btn_refine,
                                call2),
                        new Action(
                                mContext,
                                OmniboxDrawableState.forColor(0),
                                R.string.accessibility_omnibox_btn_refine,
                                call3));
        mModel.set(BaseSuggestionViewProperties.ACTION_BUTTONS, list);

        List<ImageView> actionButtons = mBaseView.getActionButtons();
        Assert.assertEquals(3, actionButtons.size());
        Assert.assertEquals(View.VISIBLE, actionButtons.get(0).getVisibility());
        Assert.assertEquals(View.VISIBLE, actionButtons.get(1).getVisibility());
        Assert.assertEquals(View.VISIBLE, actionButtons.get(2).getVisibility());

        verify(mBaseView, times(1)).addView(actionButtons.get(0));
        verify(mBaseView, times(1)).addView(actionButtons.get(1));
        verify(mBaseView, times(1)).addView(actionButtons.get(2));

        Assert.assertEquals(list.get(0).icon.drawable, actionButtons.get(0).getDrawable());
        Assert.assertEquals(list.get(1).icon.drawable, actionButtons.get(1).getDrawable());
        Assert.assertEquals(list.get(2).icon.drawable, actionButtons.get(2).getDrawable());

        Assert.assertTrue(actionButtons.get(0).performClick());
        verify(call1, times(1)).run();
        Assert.assertTrue(actionButtons.get(1).performClick());
        verify(call2, times(1)).run();
        Assert.assertTrue(actionButtons.get(2).performClick());
        verify(call3, times(1)).run();
    }

    @Test
    public void actionIcon_hideIcons() {
        final List<Action> list =
                Arrays.asList(
                        new Action(
                                mContext,
                                OmniboxDrawableState.forColor(0),
                                R.string.accessibility_omnibox_btn_refine,
                                CallbackUtils.emptyRunnable()),
                        new Action(
                                mContext,
                                OmniboxDrawableState.forColor(0),
                                R.string.accessibility_omnibox_btn_refine,
                                CallbackUtils.emptyRunnable()),
                        new Action(
                                mContext,
                                OmniboxDrawableState.forColor(0),
                                R.string.accessibility_omnibox_btn_refine,
                                CallbackUtils.emptyRunnable()));

        final List<ImageView> actionButtons = mBaseView.getActionButtons();
        mModel.set(BaseSuggestionViewProperties.ACTION_BUTTONS, list);
        Assert.assertEquals(3, actionButtons.size());
        final View actionButton1 = actionButtons.get(0);
        final View actionButton2 = actionButtons.get(1);
        final View actionButton3 = actionButtons.get(2);
        verify(mBaseView, times(1)).addView(actionButton1);
        verify(mBaseView, times(1)).addView(actionButton2);
        verify(mBaseView, times(1)).addView(actionButton3);

        mModel.set(BaseSuggestionViewProperties.ACTION_BUTTONS, list.subList(0, 2));
        Assert.assertEquals(2, actionButtons.size());
        verify(mBaseView, times(1)).removeView(actionButton3);

        mModel.set(BaseSuggestionViewProperties.ACTION_BUTTONS, list.subList(0, 1));
        Assert.assertEquals(1, actionButtons.size());
        verify(mBaseView, times(1)).removeView(actionButton2);

        mModel.set(BaseSuggestionViewProperties.ACTION_BUTTONS, null);
        Assert.assertEquals(0, actionButtons.size());
        verify(mBaseView, times(1)).removeView(actionButton1);
    }

    @Test
    public void actionIcon_dontCrashWhenRecycling() {
        // Force a dirty/recycled view that would have a button view, when the model does not carry
        // any aciton.
        Assert.assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        mBaseView.setActionButtonsCount(1);
        // Change in color scheme happening ahead of setting action could cause a crash.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.LIGHT_BRANDED_THEME);
    }

    @Test
    public void partialSuggestionRounding() {
        mModel.set(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, false);
        mModel.set(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED, true);

        Assert.assertTrue(mBaseView.getClipToOutline());
        // Expect the RoundedCornerOutlineProvider. Fail if it's anything else.
        var provider = (RoundedCornerOutlineProvider) mBaseView.getOutlineProvider();
        Assert.assertTrue(provider.isTopEdgeRounded());
        Assert.assertFalse(provider.isBottomEdgeRounded());
    }

    @Test
    public void fullSuggestionRounding() {
        mModel.set(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, true);
        mModel.set(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED, true);

        Assert.assertTrue(mBaseView.getClipToOutline());
        // Expect the RoundedCornerOutlineProvider. Fail if it's anything else.
        var provider = (RoundedCornerOutlineProvider) mBaseView.getOutlineProvider();
        Assert.assertTrue(provider.isTopEdgeRounded());
        Assert.assertTrue(provider.isBottomEdgeRounded());
    }

    @Test
    public void noSuggestionRounding() {
        mModel.set(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, false);
        mModel.set(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED, false);

        Assert.assertFalse(mBaseView.getClipToOutline());
    }

    @Test
    public void applySelectableBackground_incognito() {
        // This is a whitebox test. It currently assumes that the Suggestion background is a
        // LayerDrawable, whose bottom element represents the color.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);

        var backgroundCaptor = ArgumentCaptor.forClass(LayerDrawable.class);
        verify(mBaseView).setBackground(backgroundCaptor.capture());

        Assert.assertEquals(
                mContext.getColor(R.color.omnibox_suggestion_bg_incognito),
                ((ColorDrawable) backgroundCaptor.getValue().getDrawable(0)).getColor());
    }

    @Test
    public void applySelectableBackground_nonIncognito() {
        // This is a whitebox test. It currently assumes that the Suggestion background is a
        // LayerDrawable, whose bottom element represents the color.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.LIGHT_BRANDED_THEME);

        var backgroundCaptor = ArgumentCaptor.forClass(LayerDrawable.class);
        verify(mBaseView).setBackground(backgroundCaptor.capture());

        Assert.assertEquals(
                ChromeColors.getSurfaceColor(
                        mBaseView.getContext(), R.dimen.omnibox_suggestion_bg_elevation),
                ((ColorDrawable) backgroundCaptor.getValue().getDrawable(0)).getColor());
    }

    @Test
    public void applySelectableBackground_reuseConstantState() {
        // This is a whitebox test. It currently assumes that the Suggestion background is a
        // LayerDrawable, whose bottom element represents the color.
        var bgCaptor1 = ArgumentCaptor.forClass(LayerDrawable.class);
        var bgCaptor2 = ArgumentCaptor.forClass(LayerDrawable.class);

        // First call should instantiate incognito color.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        verify(mBaseView).setBackground(bgCaptor1.capture());

        // Attempt to re-use the background color.
        // We do this by instantiating a mock which does not deliver Context.
        // This must not crash.
        var viewWithNoContext = mock(View.class);
        BaseSuggestionViewBinder.applySelectableBackground(mModel, viewWithNoContext);
        verify(viewWithNoContext).setBackground(bgCaptor2.capture());

        var color1 = ((ColorDrawable) bgCaptor1.getValue().getDrawable(0)).getColor();
        var color2 = ((ColorDrawable) bgCaptor2.getValue().getDrawable(0)).getColor();

        Assert.assertEquals(color1, color2);
    }

    @Test
    public void applySelectableBackground_clearConstantStateWhenClientColorSchemeChanges() {
        // This is a whitebox test. It currently assumes that the Suggestion background is a
        // LayerDrawable, whose bottom element represents the color.
        var incognitoBgCaptor = ArgumentCaptor.forClass(LayerDrawable.class);
        var lightBgCaptor = ArgumentCaptor.forClass(LayerDrawable.class);

        // First call should instantiate incognito color.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        verify(mBaseView).setBackground(incognitoBgCaptor.capture());
        clearInvocations(mBaseView);

        // Second call should instantiate regular color.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.LIGHT_BRANDED_THEME);
        verify(mBaseView).setBackground(lightBgCaptor.capture());
        clearInvocations(mBaseView);

        var incognitoColor =
                ((ColorDrawable) incognitoBgCaptor.getValue().getDrawable(0)).getColor();
        var lightColor = ((ColorDrawable) lightBgCaptor.getValue().getDrawable(0)).getColor();

        Assert.assertNotEquals(incognitoColor, lightColor);
    }

    @Test
    public void applySelectableBackground_clearConstantStateWhenSystemColorSchemeChanges() {
        // This is a whitebox test. It currently assumes that the Suggestion background is a
        // LayerDrawable, whose bottom element represents the color.

        // First call should instantiate incognito color.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        Assert.assertNotNull(BaseSuggestionViewBinder.getFocusableDrawableStateForTesting());

        // Check that we're not resetting the state if neither Client nor System properties change.
        BaseSuggestionViewBinder.maybeResetCachedFocusableDrawableState(mModel, mBaseView);
        Assert.assertNotNull(BaseSuggestionViewBinder.getFocusableDrawableStateForTesting());

        // Second call should instantiate regular color.
        // Configuration change refreshes all of Chrome.
        // https://robolectric.org/device-configuration
        RuntimeEnvironment.setQualifiers("+night");

        // We've enabled night theme. Confirm that the cached state is invalidated.
        BaseSuggestionViewBinder.maybeResetCachedFocusableDrawableState(mModel, mBaseView);
        Assert.assertNull(BaseSuggestionViewBinder.getFocusableDrawableStateForTesting());
    }

    @Test
    public void applySelectableBackground_reuseConstantStateAcrossViews() {
        // This test validates, that we don't drop the cached StateDrawable whenever we create a new
        // view that declares all the same set of properties (including COLOR_SCHEME) from scratch.
        // In the event the newly created PropertyModel declares the same COLOR_SCHEME as ones
        // already built, we want to continue using the cached ConstantState.

        // First call should cache the ConstantState for the background.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.LIGHT_BRANDED_THEME);
        var state1 = BaseSuggestionViewBinder.getFocusableDrawableStateForTesting();

        // Create a second MVP setup. Use Bare context that has no theme data.
        var newModel = new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        var viewWithNoContext = spy(new BaseSuggestionView(new ImageView(mBareContext)));
        PropertyModelChangeProcessor.create(
                newModel, viewWithNoContext, new BaseSuggestionViewBinder((m, v, p) -> {}));

        // Apply the same color scheme to the new model.
        // Observe that we don't crash.
        newModel.set(
                SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.LIGHT_BRANDED_THEME);
        var state2 = BaseSuggestionViewBinder.getFocusableDrawableStateForTesting();

        Assert.assertEquals(state1, state2);
    }

    @Test
    @Config(qualifiers = "ldltr")
    public void iconPadding_ltr() {
        runDecorationIconPaddingTest();
    }

    @Test
    @Config(qualifiers = "ldrtl")
    public void iconPadding_rtl() {
        runDecorationIconPaddingTest();
    }

    @Test
    @Config(qualifiers = "ldltr")
    public void iconStartPadding_ltr() {
        runDecorationIconPaddingTest();
    }

    @Test
    @Config(qualifiers = "ldrtl")
    public void iconStartPadding_rtl() {
        runDecorationIconPaddingTest();
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    public void iconStartPadding_tablet_ltr() {
        runDecorationIconPaddingTest();
    }

    @Test
    @Config(qualifiers = "ldrtl-sw600dp")
    public void iconStartPadding_tablet_rtl() {
        runDecorationIconPaddingTest();
    }

    private void runDecorationIconPaddingTest() {
        BaseSuggestionViewBinder.initializeDimensions(mContext);

        int smallRoundingRadius =
                mResources.getDimensionPixelSize(R.dimen.omnibox_small_icon_rounding_radius);
        int largeRoundingRadius =
                mResources.getDimensionPixelSize(R.dimen.omnibox_large_icon_rounding_radius);
        int smallEdgeSize =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_24dp_icon_size);
        int largeEdgeSize =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_36dp_icon_size);

        // Variant 1: Small, wide, short icon.
        // Width bound by the edge edge size, height wrapping content.
        var b = Bitmap.createBitmap(/* width= */ 2, /* height= */ 1, Bitmap.Config.ALPHA_8);

        OmniboxDrawableState state = OmniboxDrawableState.forFavIcon(mContext, b);
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        assertEquals(MarginLayoutParams.WRAP_CONTENT, mIconView.getLayoutParams().height);
        assertEquals(smallEdgeSize, mIconView.getLayoutParams().width);
        assertEquals(smallRoundingRadius, mBaseView.decorationIconOutline.getRadiusForTesting());

        // Variant 2: Large, wide, short icon.
        // Width bound by the edge edge size, height wrapping content.
        state = OmniboxDrawableState.forImage(mContext, b);
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        assertEquals(MarginLayoutParams.WRAP_CONTENT, mIconView.getLayoutParams().height);
        assertEquals(largeEdgeSize, mIconView.getLayoutParams().width);
        assertEquals(largeRoundingRadius, mBaseView.decorationIconOutline.getRadiusForTesting());

        // Variant 3: Small, narrow, tall icon.
        // Height bound by the edge edge size, width wrapping content.
        b = Bitmap.createBitmap(/* width= */ 1, /* height= */ 2, Bitmap.Config.ALPHA_8);

        state = OmniboxDrawableState.forFavIcon(mContext, b);
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        assertEquals(MarginLayoutParams.WRAP_CONTENT, mIconView.getLayoutParams().width);
        assertEquals(smallEdgeSize, mIconView.getLayoutParams().height);
        assertEquals(smallRoundingRadius, mBaseView.decorationIconOutline.getRadiusForTesting());

        // Variant 4: Large, narrow, tall icon.
        // Height bound by the edge edge size, width wrapping content.
        state = OmniboxDrawableState.forImage(mContext, b);
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        assertEquals(MarginLayoutParams.WRAP_CONTENT, mIconView.getLayoutParams().width);
        assertEquals(largeEdgeSize, mIconView.getLayoutParams().height);
        assertEquals(largeRoundingRadius, mBaseView.decorationIconOutline.getRadiusForTesting());
    }

    @Test
    public void topPadding() {
        mModel.set(BaseSuggestionViewProperties.TOP_PADDING, 13);
        assertEquals(13, mBaseView.getPaddingTop());
    }
}
