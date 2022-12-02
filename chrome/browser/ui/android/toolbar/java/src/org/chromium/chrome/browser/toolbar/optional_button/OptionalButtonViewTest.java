// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import static junit.framework.Assert.assertFalse;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
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

import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.OneShotPreDrawListener;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonConstants.TransitionType;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonViewTest.ShadowOneShotPreDrawListener;

import java.util.function.BooleanSupplier;

/**
 * Unit tests for OptionalButtonView.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowOneShotPreDrawListener.class})
public class OptionalButtonViewTest {
    private Context mActivity;

    private OptionalButtonView mOptionalButtonView;
    private ImageButton mInnerButton;
    private TextView mActionChipLabel;
    private ImageView mButtonBackground;
    private ShadowLooper mShadowLooper;
    private BooleanSupplier mMockAnimationChecker;
    private Callback<Transition> mMockBeginDelayedTransition;

    @Before
    public void setUp() {
        mActivity = new ContextThemeWrapper(
                Robolectric.setupActivity(Activity.class), R.style.Theme_BrowserUI_DayNight);
        mMockAnimationChecker = Mockito.mock(BooleanSupplier.class);
        when(mMockAnimationChecker.getAsBoolean()).thenReturn(true);
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS,
                "action_chip_with_different_color", "false");

        FeatureList.setTestValues(testValues);

        mOptionalButtonView = (OptionalButtonView) LayoutInflater.from(mActivity).inflate(
                R.layout.optional_button_layout, null);
        mOptionalButtonView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mOptionalButtonView.setIsAnimationAllowedPredicate(mMockAnimationChecker);
        mOptionalButtonView.layout(10, 10, 10, 10);

        mMockBeginDelayedTransition = Mockito.mock(Callback.class);

        mShadowLooper = Shadows.shadowOf(Looper.getMainLooper());
        Handler handler = new Handler(Looper.getMainLooper());
        // This handler is used to schedule the action chip collapse.
        mOptionalButtonView.setHandlerForTesting(handler);

        // This function replaces calls to TransitionManager.beginDelayedTransition which is hard to
        // mock as it's static.
        mOptionalButtonView.setFakeBeginDelayedTransitionForTesting(mMockBeginDelayedTransition);

        mInnerButton = (ImageButton) mOptionalButtonView.getButtonView();
        mActionChipLabel = mOptionalButtonView.findViewById(R.id.action_chip_label);
        mButtonBackground =
                mOptionalButtonView.findViewById(R.id.swappable_icon_secondary_background);
    }

    private ButtonDataImpl getDataForStaticNewTabIconButton() {
        Drawable iconDrawable = AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        int contentDescriptionId = R.string.button_new_tab;

        // Whether a button is static or dynamic is determined by the button variant.
        ButtonSpec buttonSpec = new ButtonSpec(iconDrawable, clickListener, longClickListener,
                contentDescriptionId, true, null, /* buttonVariant= */
                AdaptiveToolbarButtonVariant.NEW_TAB, /*actionChipLabelResId=*/Resources.ID_NULL);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    private ButtonDataImpl getDataForPriceTrackingIconButton() {
        Drawable iconDrawable = AppCompatResources.getDrawable(mActivity, R.drawable.btn_mic);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        int contentDescriptionId = R.string.enable_price_tracking_menu_item;

        // Whether a button is static or dynamic is determined by the button variant.
        ButtonSpec buttonSpec = new ButtonSpec(iconDrawable, clickListener, longClickListener,
                contentDescriptionId, true, null, /* buttonVariant= */
                AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                /*actionChipLabelResId=*/Resources.ID_NULL);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    private ButtonDataImpl getDataForPriceTrackingActionChip() {
        Drawable iconDrawable = AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        int contentDescriptionId = R.string.actionbar_share;
        int actionChipLabelResId = R.string.adaptive_toolbar_button_preference_share;

        ButtonSpec buttonSpec = new ButtonSpec(iconDrawable, clickListener, longClickListener,
                contentDescriptionId, true, null, /* buttonVariant= */
                AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                /*actionChipLabelResId=*/actionChipLabelResId);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);

        return buttonData;
    }

    @Test
    public void testSetButtonEnabled() {
        ButtonDataImpl disabledButton = getDataForPriceTrackingIconButton();
        disabledButton.setEnabled(false);
        ViewGroup transitionRoot = mock(ViewGroup.class);

        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonData buttonData = getDataForPriceTrackingIconButton();
        String contentDescriptionString = mActivity.getResources().getString(
                buttonData.getButtonSpec().getContentDescriptionResId());
        ViewGroup transitionRoot = mock(ViewGroup.class);

        mOptionalButtonView.setTransitionRoot(transitionRoot);
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
        ButtonData firstButtonData = getDataForPriceTrackingIconButton();
        ButtonData secondButtonData = getDataForStaticNewTabIconButton();

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        verifyNoMoreInteractions(firstButtonData.getButtonSpec().getOnClickListener(),
                firstButtonData.getButtonSpec().getOnLongClickListener());
    }

    @Test
    public void testSetIconDrawableWithAnimation_expandActionChipFromHidden() {
        ButtonData actionChipButtonData = getDataForPriceTrackingActionChip();
        String actionChipLabel = mActivity.getResources().getString(
                actionChipButtonData.getButtonSpec().getActionChipLabelResId());

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonData actionChipButtonData = getDataForPriceTrackingActionChip();

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonData actionChipButtonData = getDataForPriceTrackingActionChip();

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonData actionChipButtonData = getDataForPriceTrackingActionChip();

        // Alternative color is controlled by a field trial param.
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
                "action_chip_with_different_color", "true");
        FeatureList.setTestValues(testValues);

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonData secondButton = getDataForPriceTrackingIconButton();
        ButtonData actionChipButton = getDataForPriceTrackingActionChip();

        Runnable beforeHideTransitionCallback = mock(Runnable.class);
        Callback<Integer> transitionStartedCallback = mock(Callback.class);
        Callback<Integer> transitionFinishedCallback = mock(Callback.class);

        mOptionalButtonView.setTransitionStartedCallback(transitionStartedCallback);
        mOptionalButtonView.setTransitionFinishedCallback(transitionFinishedCallback);
        mOptionalButtonView.setOnBeforeHideTransitionCallback(beforeHideTransitionCallback);

        InOrder inOrder = Mockito.inOrder(beforeHideTransitionCallback, transitionStartedCallback,
                transitionFinishedCallback);

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonData secondButton = getDataForPriceTrackingIconButton();
        ButtonData actionChipButton = getDataForPriceTrackingActionChip();
        when(mMockAnimationChecker.getAsBoolean()).thenReturn(false);

        Runnable beforeHideTransitionCallback = mock(Runnable.class);
        Callback<Integer> transitionStartedCallback = mock(Callback.class);
        Callback<Integer> transitionFinishedCallback = mock(Callback.class);

        mOptionalButtonView.setTransitionStartedCallback(transitionStartedCallback);
        mOptionalButtonView.setTransitionFinishedCallback(transitionFinishedCallback);
        mOptionalButtonView.setOnBeforeHideTransitionCallback(beforeHideTransitionCallback);

        InOrder inOrder = Mockito.inOrder(beforeHideTransitionCallback, transitionStartedCallback,
                transitionFinishedCallback);

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonData priceTrackingButtonData = getDataForPriceTrackingIconButton();

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

        mOptionalButtonView.updateButtonWithAnimation(priceTrackingButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        mOptionalButtonView.updateButtonWithAnimation(priceTrackingButtonData);

        // Calling updateButtonWithAnimation with the same button variant many times shouldn't begin
        // repeated animations.
        verify(mMockBeginDelayedTransition).onResult(any());
    }

    @Test
    public void testUpdateButton_sameButtonWithDifferentDrawableTriggersTransition() {
        ButtonDataImpl priceTrackingButtonData = getDataForPriceTrackingIconButton();

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

        mOptionalButtonView.updateButtonWithAnimation(priceTrackingButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        Drawable newIconDrawable =
                AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        ButtonSpec originalButtonSpec = priceTrackingButtonData.getButtonSpec();
        // Create a copy of the original ButtonSpec with a different variant.
        priceTrackingButtonData.setButtonSpec(new ButtonSpec(newIconDrawable,
                originalButtonSpec.getOnClickListener(),
                originalButtonSpec.getOnLongClickListener(),
                originalButtonSpec.getContentDescriptionResId(),
                originalButtonSpec.getSupportsTinting(), originalButtonSpec.getIPHCommandBuilder(),
                originalButtonSpec.getButtonVariant(),
                originalButtonSpec.getActionChipLabelResId()));

        mOptionalButtonView.updateButtonWithAnimation(priceTrackingButtonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Calling updateButtonWithAnimation with the same button variant but with a different
        // drawable should begin an animation.
        verify(mMockBeginDelayedTransition, times(2)).onResult(any());
    }

    @Test
    public void testUpdateButton_sameButtonWithDifferentSpecTriggersTransition() {
        ButtonDataImpl buttonData = getDataForStaticNewTabIconButton();

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Keep the same ButtonData instance, but use a different spec.
        buttonData.setButtonSpec(getDataForPriceTrackingIconButton().getButtonSpec());

        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // Calling updateButtonWithAnimation with the same ButtonData instance but with a different
        // variant many times should begin a new animation.
        verify(mMockBeginDelayedTransition, times(2)).onResult(any());
    }

    @Test
    public void testUpdateButton_sameButtonWithDifferentVisibilityTriggersTransition() {
        ButtonDataImpl buttonData = getDataForStaticNewTabIconButton();

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

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
        ButtonDataImpl buttonData = getDataForPriceTrackingActionChip();
        // Set a string that's too long as an action chip label. Real code measures the number of
        // pixels this will take on screen, but robolectric just uses the character count, so use
        // any string with more than 150 characters.
        buttonData.updateActionChipResourceId(R.string.sign_in_managed_account_description);

        int maxActionChipWidth = mOptionalButtonView.getResources().getDimensionPixelSize(
                R.dimen.toolbar_phone_optional_button_action_chip_max_width);

        ViewGroup transitionRoot = mock(ViewGroup.class);
        mOptionalButtonView.setTransitionRoot(transitionRoot);

        mOptionalButtonView.updateButtonWithAnimation(buttonData);
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Button shouldn't be wider than the established maximum.
        Assert.assertThat(mOptionalButtonView.getLayoutParams().width,
                Matchers.lessThanOrEqualTo(maxActionChipWidth));
    }

    @Test
    public void testUpdateButton_shouldWaitUntilButtonIsLaidOut() {
        ButtonDataImpl buttonData = getDataForPriceTrackingActionChip();
        ViewGroup transitionRoot = mock(ViewGroup.class);
        ShadowViewGroup shadowOptionalButtonView = Shadows.shadowOf(mOptionalButtonView);
        // Detach and re-attach to window to reset the isLaidOut() flag.
        shadowOptionalButtonView.callOnDetachedFromWindow();
        shadowOptionalButtonView.callOnAttachedToWindow();

        mOptionalButtonView.setTransitionRoot(transitionRoot);
        // Try to update the button before it's laid out.
        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // We shouldn't begin a transition yet.
        verify(mMockBeginDelayedTransition, never()).onResult(any());

        // We should have set a pre draw listener instead.
        Assert.assertNotNull(ShadowOneShotPreDrawListener.getRunnable());

        // Run that listener once the view is laid out.
        mOptionalButtonView.layout(100, 50, 10, 10);
        ShadowOneShotPreDrawListener.getRunnable().run();

        // Now we should begin our transition.
        verify(mMockBeginDelayedTransition).onResult(any());
    }

    @Test
    public void testUpdateButton_shouldIgnoreChangesWhileWaitingForLayout() {
        ButtonDataImpl buttonData = getDataForPriceTrackingActionChip();
        ViewGroup transitionRoot = mock(ViewGroup.class);
        ShadowViewGroup shadowOptionalButtonView = Shadows.shadowOf(mOptionalButtonView);
        // Detach and re-attach to window to reset the isLaidOut() flag.
        shadowOptionalButtonView.callOnDetachedFromWindow();
        shadowOptionalButtonView.callOnAttachedToWindow();

        mOptionalButtonView.setTransitionRoot(transitionRoot);
        // Try to update the button before it's laid out.
        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        // Change the attributes of buttonData without calling updateButtonWithAnimation again, this
        // should have no effect on the button's state after the transition.
        buttonData.setCanShow(false);

        // Run that listener once the view is laid out.
        mOptionalButtonView.layout(100, 50, 10, 10);
        ShadowOneShotPreDrawListener.getRunnable().run();

        // Normally called by TransitionManager.
        mOptionalButtonView.onTransitionStart(null);
        mOptionalButtonView.onTransitionEnd(null);

        // Button should be visible, the property change that happened between
        // updateButtonWithAnimation and layout is ignored.
        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(View.VISIBLE, mInnerButton.getVisibility());
        assertEquals(View.VISIBLE, mActionChipLabel.getVisibility());
    }

    @Implements(OneShotPreDrawListener.class)
    static class ShadowOneShotPreDrawListener {
        private static Runnable sRunnable;

        @Implementation
        protected static OneShotPreDrawListener add(
                @NonNull View view, @NonNull Runnable runnable) {
            sRunnable = runnable;

            return null;
        }

        static Runnable getRunnable() {
            return sRunnable;
        }
    }
}
