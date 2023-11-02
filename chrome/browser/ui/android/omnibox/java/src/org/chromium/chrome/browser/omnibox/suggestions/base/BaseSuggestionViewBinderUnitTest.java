// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link BaseSuggestionViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseSuggestionViewBinderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    BaseSuggestionView mBaseView;

    @Mock
    DecoratedSuggestionView mDecoratedView;

    @Mock
    ImageView mIconView;

    @Mock
    ImageView mContentView;

    private Activity mActivity;
    private Resources mResources;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mResources = mActivity.getResources();

        when(mContentView.getContext()).thenReturn(mActivity);

        mBaseView = spy(new BaseSuggestionView(mContentView));

        when(mBaseView.getDecoratedSuggestionView()).thenReturn(mDecoratedView);
        when(mBaseView.getSuggestionImageView()).thenReturn(mIconView);
        when(mBaseView.getContentView()).thenReturn(mContentView);
        when(mDecoratedView.getContentView()).thenReturn(mContentView);
        when(mDecoratedView.getResources()).thenReturn(mResources);
        when(mIconView.getContext()).thenReturn(mActivity);

        mModel = new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mBaseView,
                new BaseSuggestionViewBinder(
                        (m, v, p) -> { Assert.assertEquals(mContentView, v); }));
    }

    @Test
    public void decorIcon_showSquareIcon() {
        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        verify(mIconView).setClipToOutline(false);

        verify(mIconView).setVisibility(View.VISIBLE);
        verify(mIconView).setImageDrawable(state.drawable);
    }

    @Test
    public void decorIcon_showRoundedIcon() {
        SuggestionDrawableState state =
                SuggestionDrawableState.Builder.forColor(0).setUseRoundedCorners(true).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        verify(mIconView).setClipToOutline(true);

        verify(mIconView).setVisibility(View.VISIBLE);
        verify(mIconView).setImageDrawable(state.drawable);
    }

    @Test
    public void decorIcon_hideIcon() {
        InOrder ordered = inOrder(mIconView);

        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        mModel.set(BaseSuggestionViewProperties.ICON, null);

        ordered.verify(mIconView).setVisibility(View.VISIBLE);
        ordered.verify(mIconView).setImageDrawable(state.drawable);
        ordered.verify(mIconView).setVisibility(View.GONE);
        // Ensure we're releasing drawable to free memory.
        ordered.verify(mIconView).setImageDrawable(null);
    }

    @Test
    public void actionIcon_showIcon() {
        Runnable callback = mock(Runnable.class);
        List<Action> list = Arrays.asList(
                new Action(mActivity, SuggestionDrawableState.Builder.forColor(0).build(),
                        R.string.accessibility_omnibox_btn_refine, callback));
        mModel.set(BaseSuggestionViewProperties.ACTIONS, list);

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

        List<Action> list = Arrays.asList(
                new Action(mActivity, SuggestionDrawableState.Builder.forColor(0).build(),
                        R.string.accessibility_omnibox_btn_refine, call1),
                new Action(mActivity, SuggestionDrawableState.Builder.forColor(0).build(),
                        R.string.accessibility_omnibox_btn_refine, call2),
                new Action(mActivity, SuggestionDrawableState.Builder.forColor(0).build(),
                        R.string.accessibility_omnibox_btn_refine, call3));
        mModel.set(BaseSuggestionViewProperties.ACTIONS, list);

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
        final List<Action> list = Arrays.asList(
                new Action(mActivity, SuggestionDrawableState.Builder.forColor(0).build(),
                        R.string.accessibility_omnibox_btn_refine, () -> {}),
                new Action(mActivity, SuggestionDrawableState.Builder.forColor(0).build(),
                        R.string.accessibility_omnibox_btn_refine, () -> {}),
                new Action(mActivity, SuggestionDrawableState.Builder.forColor(0).build(),
                        R.string.accessibility_omnibox_btn_refine, () -> {}));

        final List<ImageView> actionButtons = mBaseView.getActionButtons();
        mModel.set(BaseSuggestionViewProperties.ACTIONS, list);
        Assert.assertEquals(3, actionButtons.size());
        final View actionButton1 = actionButtons.get(0);
        final View actionButton2 = actionButtons.get(1);
        final View actionButton3 = actionButtons.get(2);
        verify(mBaseView, times(1)).addView(actionButton1);
        verify(mBaseView, times(1)).addView(actionButton2);
        verify(mBaseView, times(1)).addView(actionButton3);

        mModel.set(BaseSuggestionViewProperties.ACTIONS, list.subList(0, 2));
        Assert.assertEquals(2, actionButtons.size());
        verify(mBaseView, times(1)).removeView(actionButton3);

        mModel.set(BaseSuggestionViewProperties.ACTIONS, list.subList(0, 1));
        Assert.assertEquals(1, actionButtons.size());
        verify(mBaseView, times(1)).removeView(actionButton2);

        mModel.set(BaseSuggestionViewProperties.ACTIONS, null);
        Assert.assertEquals(0, actionButtons.size());
        verify(mBaseView, times(1)).removeView(actionButton1);
    }

    @Test
    public void actionIcon_dontCrashWhenRecycling() {
        // Force a dirty/recycled view that would have a button view, when the model does not carry
        // any aciton.
        Assert.assertNull(mModel.get(BaseSuggestionViewProperties.ACTIONS));
        mBaseView.setActionButtonsCount(1);
        // Change in color scheme happening ahead of setting action could cause a crash.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.LIGHT_BRANDED_THEME);
    }

    @Test
    public void suggestionPadding_decorIconPresent() {
        final int startSpace = 0;
        final int endSpace = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);

        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        verify(mDecoratedView).setPaddingRelative(startSpace, 0, endSpace, 0);
        verify(mBaseView, never()).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void suggestionPadding_decorIconAbsent() {
        final int startSpace = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_start_offset_without_icon);
        final int endSpace = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);

        mModel.set(BaseSuggestionViewProperties.ICON, null);
        verify(mDecoratedView).setPaddingRelative(startSpace, 0, endSpace, 0);
        verify(mBaseView, never()).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void suggestionDensity_defaultMode() {
        mModel.set(
                BaseSuggestionViewProperties.DENSITY, BaseSuggestionViewProperties.Density.DEFAULT);
        final int expectedPadding =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_semicompact_padding);
        final int expectedHeight =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_semicompact_height);
        verify(mContentView).setPaddingRelative(0, expectedPadding, 0, expectedPadding);
        verify(mContentView).setMinimumHeight(expectedHeight);
    }

    @Test
    public void suggestionDensity_compactMode() {
        mModel.set(
                BaseSuggestionViewProperties.DENSITY, BaseSuggestionViewProperties.Density.COMPACT);
        final int expectedPadding =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_compact_padding);
        final int expectedHeight =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_compact_height);
        verify(mContentView).setPaddingRelative(0, expectedPadding, 0, expectedPadding);
        verify(mContentView).setMinimumHeight(expectedHeight);
    }

    @Test
    public void suggestionBackground() {
        mModel.set(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, false);
        mModel.set(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED, true);

        verify(mBaseView).setBackground(any());
        Assert.assertNotNull(mBaseView.getBackground());
    }

    @Test
    public void suggestionMargin() {
        mModel.set(DropdownCommonProperties.BOTTOM_MARGIN, 17);
        mModel.set(DropdownCommonProperties.TOP_MARGIN, 13);

        verify(mBaseView).setLayoutParams(any());
        int sideSpacing = mBaseView.getContext().getResources().getDimensionPixelOffset(
                R.dimen.omnibox_suggestion_side_spacing);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mBaseView.getLayoutParams();
        Assert.assertNotNull(layoutParams);
        Assert.assertEquals(sideSpacing, layoutParams.leftMargin);
        Assert.assertEquals(13, layoutParams.topMargin);
        Assert.assertEquals(sideSpacing, layoutParams.rightMargin);
        Assert.assertEquals(17, layoutParams.bottomMargin);
    }

    @Test
    public void getBackgroundDrawableColor_incognito() {
        final @ColorInt int expectedColor =
                mBaseView.getContext().getColor(R.color.omnibox_suggestion_bg_incognito);
        final boolean isIncognito = true;

        Assert.assertEquals(
                BaseSuggestionViewBinder.getBackgroundDrawableColor(isIncognito, mBaseView),
                expectedColor);
    }

    @Test
    public void getBackgroundDrawableColor_notIncognito() {
        final @ColorInt int expectedColor = ChromeColors.getSurfaceColor(
                mBaseView.getContext(), R.dimen.omnibox_suggestion_bg_elevation);
        final boolean isIncognito = false;

        Assert.assertEquals(
                BaseSuggestionViewBinder.getBackgroundDrawableColor(isIncognito, mBaseView),
                expectedColor);
    }
}
