// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.transition.Transition;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonConstants.TransitionType;
import org.chromium.ui.listmenu.ListMenuButton;

import java.util.function.BooleanSupplier;

/** Unit tests for OptionalButtonView. */
@RunWith(BaseRobolectricTestRunner.class)
public class OptionalButtonViewTest {
    private Context mActivity;

    private OptionalButtonView mOptionalButtonView;
    private ImageButton mInnerButton;
    private TextView mActionChipLabel;
    private ImageView mButtonBackground;
    private ShadowLooper mShadowLooper;
    private BooleanSupplier mMockAnimationChecker;
    private Callback<Transition> mMockBeginDelayedTransition;
    private ViewGroup mMockTransitionRoot;
    private ListMenuButton mButton;
    private ImageView mAnimationView;

    @Before
    public void setUp() {
        mActivity =
                new ContextThemeWrapper(
                        Robolectric.setupActivity(Activity.class),
                        R.style.Theme_BrowserUI_DayNight);
        mMockAnimationChecker = Mockito.mock(BooleanSupplier.class);
        when(mMockAnimationChecker.getAsBoolean()).thenReturn(true);

        mOptionalButtonView =
                (OptionalButtonView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.optional_button_layout, null);
        mOptionalButtonView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mOptionalButtonView.setIsAnimationAllowedPredicate(mMockAnimationChecker);
        mOptionalButtonView.layout(10, 10, 10, 10);

        mMockBeginDelayedTransition = Mockito.mock(Callback.class);

        mMockTransitionRoot = Mockito.mock(ViewGroup.class);
        when(mMockTransitionRoot.isLaidOut()).thenReturn(true);
        mOptionalButtonView.setTransitionRoot(mMockTransitionRoot);

        mShadowLooper = shadowOf(Looper.getMainLooper());
        Handler handler = new Handler(Looper.getMainLooper());
        // This handler is used to schedule the action chip collapse.
        mOptionalButtonView.setHandlerForTesting(handler);

        // This function replaces calls to TransitionManager.beginDelayedTransition which is hard to
        // mock as it's static.
        mOptionalButtonView.setFakeBeginDelayedTransitionForTesting(mMockBeginDelayedTransition);

        mInnerButton = (ImageButton) mOptionalButtonView.getButtonView();
        mAnimationView = mOptionalButtonView.getAnimationViewForTesting();
        mActionChipLabel = mOptionalButtonView.findViewById(R.id.action_chip_label);
        mButtonBackground =
                mOptionalButtonView.findViewById(R.id.swappable_icon_secondary_background);
        mButton = mOptionalButtonView.findViewById(R.id.optional_toolbar_button);
    }

    private ButtonDataImpl getDataForStaticNewTabIconButton() {
        Drawable iconDrawable = AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        return getDataForStaticNewTabIconButton(iconDrawable);
    }

    private ButtonDataImpl getDataForStaticNewTabIconButton(Drawable iconDrawable) {
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.button_new_tab);

        // Whether a button is static or dynamic is determined by the button variant.
        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.NEW_TAB,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ R.string.new_tab_title,
                        /* showHoverHighlight= */ true);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    private ButtonDataImpl getDataForReaderModeIconButton() {
        Drawable iconDrawable =
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_mic_white_24dp);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.reader_view_text_alt);

        // Whether a button is static or dynamic is determined by the button variant.
        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.READER_MODE,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* showHoverHighlight= */ false);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    private ButtonDataImpl getDataForReaderModeActionChip() {
        Drawable iconDrawable = AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.actionbar_share);
        int actionChipLabelResId = R.string.adaptive_toolbar_button_preference_share;

        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.READER_MODE,
                        /* actionChipLabelResId= */ actionChipLabelResId,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* showHoverHighlight= */ false);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    private ButtonDataImpl getDataForTestingTooltipText(int buttonVariant, int tooltipTextIdRes) {
        Drawable iconDrawable = AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.actionbar_share);

        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ buttonVariant,
                        0,
                        tooltipTextIdRes,
                        true);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    private ButtonDataImpl getDataForButtonWithoutTint(Drawable iconDrawable) {
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.actionbar_share);

        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        /* supportsTinting= */ false,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.UNKNOWN,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* showHoverHighlight= */ false);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    @Test
    public void testHoverTooltipText() {
        // Test whether share button tooltip Text is set correctly.
        ButtonData buttonData =
                getDataForTestingTooltipText(
                        AdaptiveToolbarButtonVariant.SHARE,
                        R.string.adaptive_toolbar_button_preference_share);
        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        Assert.assertEquals(
                "Tooltip text for share button is not as expected",
                mButton.getTooltipText(),
                mActivity
                        .getResources()
                        .getString(R.string.adaptive_toolbar_button_preference_share));

        // Test whether voice search button tooltip Text is set correctly.
        buttonData =
                getDataForTestingTooltipText(
                        AdaptiveToolbarButtonVariant.VOICE,
                        R.string.adaptive_toolbar_button_preference_voice_search);
        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        Assert.assertEquals(
                "Tooltip text for voice search button is not as expected",
                mButton.getTooltipText(),
                mActivity
                        .getResources()
                        .getString(R.string.adaptive_toolbar_button_preference_voice_search));

        // Test whether new tab button tooltip Text is set correctly.
        buttonData =
                getDataForTestingTooltipText(
                        AdaptiveToolbarButtonVariant.NEW_TAB, R.string.button_new_tab);
        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        Assert.assertEquals(
                "Tooltip text for new tab button is not as expected",
                mButton.getTooltipText(),
                mActivity.getResources().getString(R.string.button_new_tab));

        // Test whether reader mode tooltip Text is null.
        buttonData = getDataForTestingTooltipText(AdaptiveToolbarButtonVariant.READER_MODE, 0);
        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        Assert.assertEquals(
                "Tooltip text for reader mode button is not as expected",
                mButton.getTooltipText(),
                null);
    }

    @Test
    public void testSetButtonEnabled() {
        ButtonDataImpl disabledButton = getDataForReaderModeIconButton();
        disabledButton.setEnabled(false);

        mOptionalButtonView.updateButtonWithAnimation(disabledButton);

        // The enabled property should be reflected immediately.
        assertFalse(mOptionalButtonView.getButtonView().isEnabled());
    }

    @Test
    public void testSetPaddingStart() {
        mOptionalButtonView.setPaddingStart(42);

        assertEquals(42, mOptionalButtonView.getPaddingStart());
    }

    @Test(expected = IllegalStateException.class)
    public void testSetIconDrawableWithAnimation_throwsExceptionWithNoTransitionRoot() {
        mOptionalButtonView.setTransitionRoot(null);

        // If we don't set a transitionRoot then we can't perform transitions.
        mOptionalButtonView.updateButtonWithAnimation(null);
    }

    @Test(expected = IllegalStateException.class)
    public void testSetIconDrawableWithAnimation_throwsExceptionWithNoAnimationPredicate() {
        // If we don't set an isAnimationAllowedPredicate then we can't perform transitions.
        mOptionalButtonView.setIsAnimationAllowedPredicate(null);
        mOptionalButtonView.updateButtonWithAnimation(null);
    }

    @Test
    public void testSetIconDrawableWithAnimation_fromHiddenToIcon() {
        ButtonData buttonData = getDataForReaderModeIconButton();
        String contentDescriptionString = buttonData.getButtonSpec().getContentDescription();

        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(View.VISIBLE, mInnerButton.getVisibility());
        assertEquals(buttonData.getButtonSpec().getDrawable(), mInnerButton.getDrawable());
        assertEquals(contentDescriptionString, mInnerButton.getContentDescription());
        // Dynamic buttons have a background.
        assertEquals(View.VISIBLE, mButtonBackground.getVisibility());
        assertEquals(View.GONE, mActionChipLabel.getVisibility());
    }

    @Test
    public void testSetIconDrawableWithAnimation_swapIcons() {
        ButtonData firstButtonData = getDataForReaderModeIconButton();
        ButtonData secondButtonData = getDataForStaticNewTabIconButton();

        // Transition from hidden to firstIcon.
        mOptionalButtonView.updateButtonWithAnimation(firstButtonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from firstIcon to secondIcon.
        mOptionalButtonView.updateButtonWithAnimation(secondButtonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);

        // In the middle of the transition all handlers should be disabled.
        mInnerButton.performClick();
        mInnerButton.performLongClick();

        mOptionalButtonView.onTransitionEnd(null);

        mInnerButton.performClick();
        mInnerButton.performLongClick();

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(View.VISIBLE, mInnerButton.getVisibility());
        assertEquals(secondButtonData.getButtonSpec().getDrawable(), mInnerButton.getDrawable());
        // Second button is static, it has no background.
        assertEquals(View.GONE, mButtonBackground.getVisibility());
        verify(secondButtonData.getButtonSpec().getOnClickListener()).onClick(any());
        verify(secondButtonData.getButtonSpec().getOnLongClickListener()).onLongClick(any());
        verifyNoMoreInteractions(
                firstButtonData.getButtonSpec().getOnClickListener(),
                firstButtonData.getButtonSpec().getOnLongClickListener());
    }

    @Test
    public void testSetIconDrawableWithAnimation_swapIcons_withAndWithoutTint() {

        // First button has an icon that supports tinting (e.g. new tab).
        Drawable firstButtonIcon =
                AppCompatResources.getDrawable(mActivity, R.drawable.star_outline_24dp);
        ButtonData firstButtonDataWithTint = getDataForStaticNewTabIconButton(firstButtonIcon);

        // Second button has an icon that doesn't support tinting (e.g. user profile pic).
        Drawable secondButtonIcon =
                AppCompatResources.getDrawable(mActivity, R.drawable.btn_star_filled);
        ButtonData secondButtonDataWithoutTint = getDataForButtonWithoutTint(secondButtonIcon);

        // This tint color comes from the current dynamic color or set by a website.
        ColorStateList tintColor = ColorStateList.valueOf(Color.RED);
        mOptionalButtonView.setColorStateList(tintColor);

        // Transition from hidden to firstIcon.
        mOptionalButtonView.updateButtonWithAnimation(firstButtonDataWithTint);

        // Start transition, normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);

        // Record the drawable being animated and its tint color.
        Drawable buttonDrawableWhileShowing = mInnerButton.getDrawable();
        ColorStateList buttonTintWhileShowing = ImageViewCompat.getImageTintList(mInnerButton);

        // Finish transition, normally called by TransitionManager.
        mOptionalButtonView.onTransitionEnd(null);

        // Record the button's drawable and tint color after the animation.
        Drawable buttonDrawableAfterShowing = mInnerButton.getDrawable();
        ColorStateList buttonTintAfterShowing = ImageViewCompat.getImageTintList(mInnerButton);

        // Transition from the tinted button to the one that doesn't support tint.
        mOptionalButtonView.updateButtonWithAnimation(secondButtonDataWithoutTint);

        // Start transition, Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);

        // During the swap animation we show both buttons at the same time, in this case
        // mInnerButton is hiding the previous tinted icon, while mAnimationView is showing the new
        // untinted icon.
        // Record both drawables and their tints.
        Drawable animationDrawableWhileSwapping = mAnimationView.getDrawable();
        ColorStateList animationTintWhileSwapping =
                ImageViewCompat.getImageTintList(mAnimationView);
        Drawable buttonDrawableWhileSwapping = mInnerButton.getDrawable();
        ColorStateList buttonTintWhileSwapping = ImageViewCompat.getImageTintList(mInnerButton);

        // Finish transition, normally called by TransitionManager.
        mOptionalButtonView.onTransitionEnd(null);

        // After the animation only mInnerButton remains visible, it instantly changes its drawable
        // and tint to the new icon.
        Drawable buttonDrawableAfterSwapping = mInnerButton.getDrawable();
        ColorStateList buttonTintAfterSwapping = ImageViewCompat.getImageTintList(mInnerButton);

        // Verify that while the first button is appearing it is shown with tint.
        assertEquals(firstButtonIcon, buttonDrawableWhileShowing);
        assertEquals(tintColor, buttonTintWhileShowing);

        // Verify that after the first button appeared it maintained its tint.
        assertEquals(firstButtonIcon, buttonDrawableAfterShowing);
        assertEquals(tintColor, buttonTintAfterShowing);

        // Verify that while the second button replaced the first one each one had the correct tint.
        assertEquals(secondButtonIcon, animationDrawableWhileSwapping);
        assertNull(animationTintWhileSwapping);
        assertEquals(firstButtonIcon, buttonDrawableWhileSwapping);
        assertEquals(tintColor, buttonTintWhileSwapping);

        // Verify that after swapping the second button remained untinted.
        assertEquals(secondButtonIcon, buttonDrawableAfterSwapping);
        assertNull(buttonTintAfterSwapping);
    }

    @Test
    public void testSetIconDrawableWithAnimation_expandActionChipFromHidden() {
        ButtonData actionChipButtonData = getDataForReaderModeActionChip();
        String actionChipLabel =
                mActivity
                        .getResources()
                        .getString(actionChipButtonData.getButtonSpec().getActionChipLabelResId());

        // Setting an action chip label string indicates that the button should transition to an
        // action chip
        mOptionalButtonView.updateButtonWithAnimation(actionChipButtonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        // In the middle of the expand transition we shouldn't listen to clicks.
        mInnerButton.performClick();
        mOptionalButtonView.onTransitionEnd(null);

        // Click listener should be set when the expand transition is finished.
        mInnerButton.performClick();

        verify(actionChipButtonData.getButtonSpec().getOnClickListener(), times(1)).onClick(any());
        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(View.VISIBLE, mInnerButton.getVisibility());
        assertEquals(View.VISIBLE, mActionChipLabel.getVisibility());
        assertEquals(actionChipLabel, mActionChipLabel.getText());
    }

    @Test
    public void testSetIconDrawableWithAnimation_expandAndCollapseActionChipFromHidden() {
        ButtonData actionChipButtonData = getDataForReaderModeActionChip();

        // Setting an action chip label string indicates that the button should transition to an
        // action chip
        mOptionalButtonView.updateButtonWithAnimation(actionChipButtonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Advance looper to begin collapse transition.
        mShadowLooper.runOneTask();

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        // Click listener should remain active during this transition.
        mInnerButton.performClick();
        mOptionalButtonView.onTransitionEnd(null);

        // Click listener should remain the same after the transition
        mInnerButton.performClick();

        verify(actionChipButtonData.getButtonSpec().getOnClickListener(), times(2)).onClick(any());
        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(View.VISIBLE, mInnerButton.getVisibility());
        assertEquals(View.GONE, mActionChipLabel.getVisibility());
    }

    @Test
    public void testSetIconDrawableWithAnimation_expandActionChipFromAnotherIcon() {
        ButtonData staticButtonData = getDataForStaticNewTabIconButton();
        ButtonData actionChipButtonData = getDataForReaderModeActionChip();

        // Transition from hidden to staticButton.
        mOptionalButtonView.updateButtonWithAnimation(staticButtonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from firstIcon to an action chip with secondIcon.
        mOptionalButtonView.updateButtonWithAnimation(actionChipButtonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        // Click button in the middle of the action chip expansion transition, nothing should
        // happen.
        mInnerButton.performClick();
        mOptionalButtonView.onTransitionEnd(null);

        // Second click listener should be set after action chip expansion is finished.
        mInnerButton.performClick();

        verify(actionChipButtonData.getButtonSpec().getOnClickListener(), times(1)).onClick(any());
        verifyNoMoreInteractions(staticButtonData.getButtonSpec().getOnClickListener());
        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(View.VISIBLE, mInnerButton.getVisibility());
        assertEquals(View.VISIBLE, mActionChipLabel.getVisibility());
    }

    @Test
    public void testUpdateButtonWithAnimation_actionChipWithAlternativeColor() {
        ButtonData actionChipButtonData = getDataForReaderModeActionChip();

        // Outside of tests alternative color is controlled by a field trial param for each button
        // variant.
        AdaptiveToolbarFeatures.setAlternativeColorOverrideForTesting(
                actionChipButtonData.getButtonSpec().getButtonVariant(), true);

        // Transition from hidden to action chip
        mOptionalButtonView.updateButtonWithAnimation(actionChipButtonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        ColorFilter filterAfterExpansion = mButtonBackground.getColorFilter();

        // Advance looper to begin collapse transition.
        mShadowLooper.runOneTask();
        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        ColorFilter filterAfterCollapse = mButtonBackground.getColorFilter();

        Assert.assertNotNull(filterAfterCollapse);
        Assert.assertNotNull(filterAfterExpansion);
        Assert.assertNotEquals(filterAfterCollapse, filterAfterExpansion);
    }

    @Test
    public void testSetIconDrawableWithAnimation_hideIcon() {
        ButtonData buttonData = getDataForStaticNewTabIconButton();

        // Transition from hidden to firstIcon.
        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Setting the icon drawable to null should trigger a hiding transition.
        mOptionalButtonView.updateButtonWithAnimation(null);

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Button should now be hidden, and its callbacks should be removed.
        mInnerButton.performClick();

        verify(buttonData.getButtonSpec().getOnClickListener(), never()).onClick(any());
        assertEquals(View.GONE, mOptionalButtonView.getVisibility());
        assertEquals(View.GONE, mInnerButton.getVisibility());
        assertEquals(View.GONE, mActionChipLabel.getVisibility());
    }

    @Test
    public void testTransitionCallbacks() {
        ButtonData firstButton = getDataForStaticNewTabIconButton();
        ButtonData secondButton = getDataForReaderModeIconButton();
        ButtonData actionChipButton = getDataForReaderModeActionChip();

        Runnable beforeHideTransitionCallback = mock(Runnable.class);
        Callback<Integer> transitionStartedCallback = mock(Callback.class);
        Callback<Integer> transitionFinishedCallback = mock(Callback.class);

        mOptionalButtonView.setTransitionStartedCallback(transitionStartedCallback);
        mOptionalButtonView.setTransitionFinishedCallback(transitionFinishedCallback);
        mOptionalButtonView.setOnBeforeHideTransitionCallback(beforeHideTransitionCallback);

        InOrder inOrder =
                Mockito.inOrder(
                        beforeHideTransitionCallback,
                        transitionStartedCallback,
                        transitionFinishedCallback);

        // Transition from hidden to firstButton.
        mOptionalButtonView.updateButtonWithAnimation(firstButton);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from firstButton to secondButton
        mOptionalButtonView.updateButtonWithAnimation(secondButton);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition back to firstButton.
        mOptionalButtonView.updateButtonWithAnimation(firstButton);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from secondButton to actionChipButton
        mOptionalButtonView.updateButtonWithAnimation(actionChipButton);
        // Run callbacks for expansion transition.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Advance looper to begin collapse transition.
        mShadowLooper.runOneTask();
        // Run callbacks for collapse transition.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from actionChipButton to hidden.
        mOptionalButtonView.updateButtonWithAnimation(null);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Verify that callbacks are called in the expected order with the right arguments.
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.SWAPPING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.SWAPPING);
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.SWAPPING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.SWAPPING);
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.EXPANDING_ACTION_CHIP);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.EXPANDING_ACTION_CHIP);
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.COLLAPSING_ACTION_CHIP);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.COLLAPSING_ACTION_CHIP);
        inOrder.verify(beforeHideTransitionCallback).run();
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.HIDING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.HIDING);
    }

    @Test
    public void testTransitionCallbacks_withAnimationDisabled() {
        ButtonData firstButton = getDataForStaticNewTabIconButton();
        ButtonData secondButton = getDataForReaderModeIconButton();
        ButtonData actionChipButton = getDataForReaderModeActionChip();
        when(mMockAnimationChecker.getAsBoolean()).thenReturn(false);

        Runnable beforeHideTransitionCallback = mock(Runnable.class);
        Callback<Integer> transitionStartedCallback = mock(Callback.class);
        Callback<Integer> transitionFinishedCallback = mock(Callback.class);

        mOptionalButtonView.setTransitionStartedCallback(transitionStartedCallback);
        mOptionalButtonView.setTransitionFinishedCallback(transitionFinishedCallback);
        mOptionalButtonView.setOnBeforeHideTransitionCallback(beforeHideTransitionCallback);

        InOrder inOrder =
                Mockito.inOrder(
                        beforeHideTransitionCallback,
                        transitionStartedCallback,
                        transitionFinishedCallback);

        // Transition from hidden to firstButton.
        mOptionalButtonView.updateButtonWithAnimation(firstButton);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from firstButton to secondButton
        mOptionalButtonView.updateButtonWithAnimation(secondButton);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition back to firstButton.
        mOptionalButtonView.updateButtonWithAnimation(firstButton);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from secondButton to actionChipButton, when animations are disabled we don't
        // expand the action chip, as the width change would look jarring. Instead we just update
        // its icon.
        mOptionalButtonView.updateButtonWithAnimation(actionChipButton);
        // Run callbacks for update transition.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Transition from actionChipButton to hidden.
        mOptionalButtonView.updateButtonWithAnimation(null);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Verify that callbacks are called in the expected order with the right arguments,
        // non-animated updates use either SHOWING or HIDING.
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.SHOWING);
        inOrder.verify(beforeHideTransitionCallback).run();
        inOrder.verify(transitionStartedCallback).onResult(TransitionType.HIDING);
        inOrder.verify(transitionFinishedCallback).onResult(TransitionType.HIDING);
    }

    @Test
    public void testUpdateButton_earlyReturnIfNothingChanged() {
        ButtonData firstButton = getDataForStaticNewTabIconButton();

        mOptionalButtonView.updateButtonWithAnimation(firstButton);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        mOptionalButtonView.updateButtonWithAnimation(firstButton);

        // Calling updateButtonWithAnimation with the same button many times shouldn't begin
        // repeated animations.
        verify(mMockBeginDelayedTransition).onResult(any());
    }

    @Test
    public void testUpdateButton_earlyReturnIfSameVariant() {
        ButtonData readerModeButtonData = getDataForReaderModeIconButton();

        mOptionalButtonView.updateButtonWithAnimation(readerModeButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        mOptionalButtonView.updateButtonWithAnimation(readerModeButtonData);

        // Calling updateButtonWithAnimation with the same button variant many times shouldn't begin
        // repeated animations.
        verify(mMockBeginDelayedTransition).onResult(any());
    }

    @Test
    public void testUpdateButton_sameButtonWithDifferentDrawableTriggersTransition() {
        ButtonDataImpl readerModeButtonData = getDataForReaderModeIconButton();

        mOptionalButtonView.updateButtonWithAnimation(readerModeButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        Drawable newIconDrawable =
                AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        ButtonSpec originalButtonSpec = readerModeButtonData.getButtonSpec();
        // Create a copy of the original ButtonSpec with a different variant.
        readerModeButtonData.setButtonSpec(
                new ButtonSpec(
                        newIconDrawable,
                        originalButtonSpec.getOnClickListener(),
                        originalButtonSpec.getOnLongClickListener(),
                        originalButtonSpec.getContentDescription(),
                        originalButtonSpec.getSupportsTinting(),
                        originalButtonSpec.getIPHCommandBuilder(),
                        originalButtonSpec.getButtonVariant(),
                        originalButtonSpec.getActionChipLabelResId(),
                        originalButtonSpec.getHoverTooltipTextId(),
                        originalButtonSpec.getShouldShowHoverHighlight()));

        mOptionalButtonView.updateButtonWithAnimation(readerModeButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Calling updateButtonWithAnimation with the same button variant but with a different
        // drawable should begin an animation.
        verify(mMockBeginDelayedTransition, times(2)).onResult(any());
    }

    @Test
    public void testUpdateButton_sameButtonWithDifferentSpecTriggersTransition() {
        ButtonDataImpl buttonData = getDataForStaticNewTabIconButton();

        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Keep the same ButtonData instance, but use a different spec.
        buttonData.setButtonSpec(getDataForReaderModeIconButton().getButtonSpec());

        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // Calling updateButtonWithAnimation with the same ButtonData instance but with a different
        // variant many times should begin a new animation.
        verify(mMockBeginDelayedTransition, times(2)).onResult(any());
    }

    @Test
    public void testUpdateButton_sameButtonWithDifferentVisibilityTriggersTransition() {
        ButtonDataImpl buttonData = getDataForStaticNewTabIconButton();

        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Keep the same ButtonData instance, but set it to not show.
        buttonData.setCanShow(false);

        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // Calling updateButtonWithAnimation with the same ButtonData instance but with a different
        // visibility many times should begin a new animation.
        verify(mMockBeginDelayedTransition, times(2)).onResult(any());
    }

    @Test
    public void testUpdateButton_actionChipWidthIsRestricted() {
        ButtonDataImpl buttonData = getDataForReaderModeActionChip();
        // Set a string that's too long as an action chip label. Real code measures the number of
        // pixels this will take on screen, but robolectric just uses the character count, so use
        // any string with more than 150 characters.
        buttonData.updateActionChipResourceId(R.string.sync_encryption_create_passphrase);

        int maxActionChipWidth =
                mOptionalButtonView
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.toolbar_phone_optional_button_action_chip_max_width);

        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Button shouldn't be wider than the established maximum.
        assertThat(
                mOptionalButtonView.getLayoutParams().width,
                Matchers.lessThanOrEqualTo(maxActionChipWidth));
    }

    @Test
    public void testUpdateButton_shouldWaitUntilTransitionRootIsLaidOut() {
        ButtonDataImpl buttonData = getDataForReaderModeActionChip();

        // Set transition root to be not laid out. If this happens then TransitionManager won't run
        // any transitions.
        when(mMockTransitionRoot.isLaidOut()).thenReturn(false);

        // Try to update the button before the transition root is laid out.
        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // We shouldn't begin a transition yet.
        verify(mMockBeginDelayedTransition, never()).onResult(any());

        // Run that listener once the view is laid out.
        when(mMockTransitionRoot.isLaidOut()).thenReturn(true);
        mOptionalButtonView.getViewTreeObserver().dispatchOnGlobalLayout();

        // Now we should begin our transition.
        verify(mMockBeginDelayedTransition).onResult(any());
    }

    @Test
    public void testUpdateButton_shouldIgnoreChangesWhileWaitingForLayout() {
        ButtonDataImpl buttonData = getDataForReaderModeActionChip();

        // Set transition root to be not laid out. If this happens then TransitionManager won't run
        // any transitions.
        when(mMockTransitionRoot.isLaidOut()).thenReturn(false);

        // Try to update the button before its transition root is laid out.
        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // Change the attributes of buttonData without calling updateButtonWithAnimation again, this
        // should have no effect on the button's state after the transition.
        buttonData.setCanShow(false);

        // Run that listener once the view is laid out.
        when(mMockTransitionRoot.isLaidOut()).thenReturn(true);
        mOptionalButtonView.getViewTreeObserver().dispatchOnGlobalLayout();

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Button should be visible, the property change that happened between
        // updateButtonWithAnimation and layout is ignored.
        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(View.VISIBLE, mInnerButton.getVisibility());
        assertEquals(View.VISIBLE, mActionChipLabel.getVisibility());
    }

    @Test
    public void testUpdateButton_shouldHideActionChipLabelWhenUpdatingWithoutAnimations() {
        ButtonDataImpl actionChipButtonData = getDataForReaderModeActionChip();
        ButtonDataImpl regularButtonData = getDataForStaticNewTabIconButton();

        // Allow animations
        when(mMockAnimationChecker.getAsBoolean()).thenReturn(true);

        // Show action chip.
        mOptionalButtonView.updateButtonWithAnimation(actionChipButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Block animations.
        when(mMockAnimationChecker.getAsBoolean()).thenReturn(false);

        // Switch to regular button
        mOptionalButtonView.updateButtonWithAnimation(regularButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Action chip shouldn't be visible
        assertEquals(View.GONE, mActionChipLabel.getVisibility());
    }

    @Test
    public void testUpdateButton_sameVariantUpdatesShouldNotBeAnimated() {
        ArgumentCaptor<Transition> transitionArgumentCaptor =
                ArgumentCaptor.forClass(Transition.class);
        // Create two ButtonData objects for the same variant (NEW_TAB) with different icons.
        ButtonDataImpl newTabButtonData =
                getDataForStaticNewTabIconButton(
                        AppCompatResources.getDrawable(mActivity, R.drawable.star_outline_24dp));
        ButtonDataImpl updatedNewTabButtonData =
                getDataForStaticNewTabIconButton(
                        AppCompatResources.getDrawable(mActivity, R.drawable.btn_star_filled));

        // First show the first icon.
        mOptionalButtonView.updateButtonWithAnimation(newTabButtonData);

        verify(mMockBeginDelayedTransition).onResult(transitionArgumentCaptor.capture());
        // Going from no button to a button should be animated.
        assertNotEquals(0, transitionArgumentCaptor.getValue().getDuration());
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Now show the second icon, with the same variant but a different drawable.
        mOptionalButtonView.updateButtonWithAnimation(updatedNewTabButtonData);

        verify(mMockBeginDelayedTransition, times(2)).onResult(transitionArgumentCaptor.capture());
        // Updating the drawable without changing variant should not be animated.
        assertEquals(0, transitionArgumentCaptor.getValue().getDuration());
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Now hide the button.
        mOptionalButtonView.updateButtonWithAnimation(null);
        verify(mMockBeginDelayedTransition, times(3)).onResult(transitionArgumentCaptor.capture());
        // Hiding the button should be animated.
        assertNotEquals(0, transitionArgumentCaptor.getValue().getDuration());
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);
    }

    @Test
    public void testUpdateButton_differentVariantUpdatesShouldBeAnimated() {
        ArgumentCaptor<Transition> transitionArgumentCaptor =
                ArgumentCaptor.forClass(Transition.class);
        // Create two ButtonData objects with different variants.
        ButtonData newTabButtonData = getDataForStaticNewTabIconButton();
        ButtonData readerModeButtonData = getDataForReaderModeIconButton();

        // First show the new tab variant.
        mOptionalButtonView.updateButtonWithAnimation(newTabButtonData);

        verify(mMockBeginDelayedTransition).onResult(transitionArgumentCaptor.capture());
        // Going from no button to a button should be animated.
        assertNotEquals(0, transitionArgumentCaptor.getValue().getDuration());
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Now show the reader mode button.
        mOptionalButtonView.updateButtonWithAnimation(readerModeButtonData);

        verify(mMockBeginDelayedTransition, times(2)).onResult(transitionArgumentCaptor.capture());
        // Changing variants should be animated.
        assertNotEquals(0, transitionArgumentCaptor.getValue().getDuration());
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);
    }
}
