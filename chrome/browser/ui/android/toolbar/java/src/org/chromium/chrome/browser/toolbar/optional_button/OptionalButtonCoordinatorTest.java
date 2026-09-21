// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator.TransitionType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.function.BooleanSupplier;

/** Unit tests for OptionalButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class OptionalButtonCoordinatorTest {
    public static final int ACTION_CHIP_COLLAPSE_DELAY_MS = 6000;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ViewGroup mMockRootView;
    @Mock private BooleanSupplier mMockIsAnimationAllowedDelegate;
    @Mock private OptionalButtonView mMockOptionalButtonView;
    @Mock private UserEducationHelper mMockUserEducationHelper;
    @Mock private Tracker mMockTracker;
    @Mock private View mView;
    @Mock private Drawable mDrawable;
    @Mock private IphCommandBuilder mIphCommandBuilder;

    @Captor ArgumentCaptor<Callback<Integer>> mCallbackArgumentCaptor;
    @Captor ArgumentCaptor<ViewRectProvider> mViewRectProviderCaptor;
    @Captor private ArgumentCaptor<Runnable> mOnShowCallbackCaptor;
    @Captor private ArgumentCaptor<Runnable> mOnDismissCallbackCaptor;

    OptionalButtonCoordinator mOptionalButtonCoordinator;

    @Before
    public void setUp() {
        doReturn(ApplicationProvider.getApplicationContext())
                .when(mMockOptionalButtonView)
                .getContext();
        mOptionalButtonCoordinator =
                new OptionalButtonCoordinator(
                        mMockOptionalButtonView,
                        () -> mMockUserEducationHelper,
                        mMockRootView,
                        mMockIsAnimationAllowedDelegate,
                        ObservableSuppliers.createNonNull(mMockTracker));
    }

    @Test
    public void testSetOnBeforeHideTransitionCallback() {
        Runnable callback = () -> {};

        mOptionalButtonCoordinator.setOnBeforeHideTransitionCallback(callback);

        verify(mMockOptionalButtonView).setOnBeforeHideTransitionCallback(callback);
    }

    @Test
    public void testSetTransitionStartedCallback() {
        Callback<Integer> callback = result -> {};

        mOptionalButtonCoordinator.setTransitionStartedCallback(callback);

        verify(mMockOptionalButtonView).setTransitionStartedCallback(callback);
    }

    @Test
    public void testSetTransitionFinishedCallback() {
        // On its constructor OptionalButtonCoordinator sets its own transition finished callback.
        verify(mMockOptionalButtonView)
                .setTransitionFinishedCallback(mCallbackArgumentCaptor.capture());
        Callback<Integer> internalCallback = mCallbackArgumentCaptor.getValue();

        Callback<Integer> externalCallback = MockitoHelper.mockCallback();

        // Set a callback.
        mOptionalButtonCoordinator.setTransitionFinishedCallback(externalCallback);

        // This callback won't be passed to the view, it'll be wrapped by the Coordinator's own
        // callback.
        verify(mMockOptionalButtonView, never()).setTransitionFinishedCallback(externalCallback);

        // Check that the external callback is wrapped by the internal one.
        internalCallback.onResult(5);
        verify(externalCallback).onResult(5);
    }

    @Test
    public void testSetBackgroundColorFilter() {
        mOptionalButtonCoordinator.setBackgroundColorFilter(Color.GREEN);

        verify(mMockOptionalButtonView).setBackgroundColorFilter(Color.GREEN);
    }

    @Test
    public void testSetBrandedColorScheme() {
        clearInvocations(mMockOptionalButtonView);
        mOptionalButtonCoordinator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);

        verify(mMockOptionalButtonView)
                .setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        verify(mMockOptionalButtonView).setColorStateList(any());
    }

    @Test
    public void testSetPaddingStart() {
        mOptionalButtonCoordinator.setPaddingStart(42);

        verify(mMockOptionalButtonView).setPaddingStart(42);
    }

    @Test
    public void testCancelTransition() {
        mOptionalButtonCoordinator.cancelTransition();
        mOptionalButtonCoordinator.cancelTransition();

        verify(mMockOptionalButtonView, times(2)).cancelTransition();
    }

    @Test
    public void testGetViewVisibility() {
        when(mMockOptionalButtonView.getVisibility()).thenReturn(View.VISIBLE);

        assertEquals(View.VISIBLE, mOptionalButtonCoordinator.getViewVisibility());

        verify(mMockOptionalButtonView).getVisibility();
    }

    @Test
    public void testGetViewWidth() {
        when(mMockOptionalButtonView.getWidth()).thenReturn(100);

        assertEquals(100, mOptionalButtonCoordinator.getViewWidth());

        verify(mMockOptionalButtonView).getWidth();
    }

    @Test
    public void testGetViewForDrawing() {
        assertEquals(mMockOptionalButtonView, mOptionalButtonCoordinator.getViewForDrawing());
    }

    @Test
    public void testGetButtonView() {
        when(mMockOptionalButtonView.getButtonView()).thenReturn(mView);

        assertEquals(mView, mOptionalButtonCoordinator.getButtonView());

        verify(mMockOptionalButtonView).getButtonView();
    }

    @Test
    public void testUpdateButton_hasErrorBadge() {
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        OnLongClickListener longClickListener = ViewUtils.emptyLongClickListener();
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setOnLongClickListener(longClickListener)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .setHasErrorBadge(true)
                        .build();
        ButtonDataImpl buttonData = new ButtonDataImpl(/* canShow= */ false, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
    }

    @Test
    public void testUpdateButton_backgroundVisible() {
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        doReturn(View.VISIBLE).when(mView).getVisibility();
        doReturn(mView).when(mMockOptionalButtonView).getBackgroundView();

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        // IPH command builder must be populated with view specific properties.
        verify(mIphCommandBuilder).setAnchorView(eq(mView));
        verify(mIphCommandBuilder).setViewRectProvider(mViewRectProviderCaptor.capture());
        assertEquals(mView, mViewRectProviderCaptor.getValue().getViewForTesting());
        verify(mIphCommandBuilder).setHighlightParams(any());
        verify(mIphCommandBuilder).setOnShowCallback(any());
        verify(mIphCommandBuilder).setOnDismissCallback(any());
        verifyNoMoreInteractions(mIphCommandBuilder);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
    }

    @Test
    public void testUpdateButton_backgroundGone() {
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        doReturn(View.GONE).when(mView).getVisibility();
        doReturn(mView).when(mMockOptionalButtonView).getBackgroundView();

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        // IPH command builder must be populated with view specific properties.
        verify(mIphCommandBuilder).setAnchorView(eq(mMockOptionalButtonView));
        verify(mIphCommandBuilder).setViewRectProvider(mViewRectProviderCaptor.capture());
        assertEquals(
                mMockOptionalButtonView, mViewRectProviderCaptor.getValue().getViewForTesting());
        verify(mIphCommandBuilder).setHighlightParams(any());
        verify(mIphCommandBuilder).setOnShowCallback(any());
        verify(mIphCommandBuilder).setOnDismissCallback(any());
        verifyNoMoreInteractions(mIphCommandBuilder);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
    }

    @Test
    public void testUpdateButton_showingIphChangesBackgroundAlpha() {
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        verify(mIphCommandBuilder).setOnShowCallback(mOnShowCallbackCaptor.capture());
        verify(mIphCommandBuilder).setOnDismissCallback(mOnDismissCallbackCaptor.capture());

        // Showing an IPH should make the background transparent to be able to see the highlight.
        mOnShowCallbackCaptor.getValue().run();
        verify(mMockOptionalButtonView).setBackgroundAlpha(0);

        // Dismissing the IPH should bring back the background to normal.
        mOnDismissCallbackCaptor.getValue().run();
        verify(mMockOptionalButtonView, atLeastOnce()).setBackgroundAlpha(255);
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsRemovedWhenNotInVariant() {
        AdaptiveToolbarFeatures.setIsDynamicActionForTesting(
                AdaptiveToolbarButtonVariant.TEST_BUTTON, true);
        FeatureOverrides.overrideParam(
                AdaptiveToolbarFeatures.CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME,
                "action_chip",
                false);

        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        int actionChipResourceId = 987654;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.TEST_BUTTON)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
        assertEquals(Resources.ID_NULL, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsRemovedByFeatureEngagement() {
        AdaptiveToolbarFeatures.setIsDynamicActionForTesting(
                AdaptiveToolbarButtonVariant.TEST_BUTTON, true);
        FeatureOverrides.overrideParam(
                AdaptiveToolbarFeatures.CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME,
                "action_chip",
                true);

        doReturn(true).when(mMockTracker).isInitialized();
        doReturn(false)
                .when(mMockTracker)
                .shouldTriggerHelpUi(FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP);

        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        int actionChipResourceId = 987654;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.TEST_BUTTON)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
        assertEquals(Resources.ID_NULL, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsKeptByFeatureEngagement() {
        AdaptiveToolbarFeatures.setIsDynamicActionForTesting(
                AdaptiveToolbarButtonVariant.TEST_BUTTON, true);
        FeatureOverrides.overrideParam(
                AdaptiveToolbarFeatures.CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME,
                "action_chip",
                true);

        doReturn(true).when(mMockTracker).isInitialized();
        doReturn(true)
                .when(mMockTracker)
                .shouldTriggerHelpUi(FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP);

        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        int actionChipResourceId = 987654;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.TEST_BUTTON)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
        assertEquals(actionChipResourceId, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsKeptForGlic() {
        AdaptiveToolbarFeatures.setIsDynamicActionForTesting(
                AdaptiveToolbarButtonVariant.GLIC, true);
        FeatureOverrides.overrideParam(
                AdaptiveToolbarFeatures.CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME,
                "action_chip",
                true);

        doReturn(true).when(mMockTracker).isInitialized();
        doReturn(false)
                .when(mMockTracker)
                .shouldTriggerHelpUi(FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP);

        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        int actionChipResourceId = 987654;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.GLIC)
                        .setHoverTooltipTextId(Resources.ID_NULL)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
        assertEquals(actionChipResourceId, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_disableButtonWithoutChanges() {
        when(mMockOptionalButtonView.getButtonView()).thenReturn(mView);

        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .build();
        ButtonDataImpl buttonData =
                new ButtonDataImpl(/* canShow= */ true, /* isEnabled= */ true, buttonSpec);

        // Call update button with an enabled button.
        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        buttonData.setEnabled(false);

        // Call updateButton with the same data, but with enabled = false.
        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        // Button should be disabled.
        verify(mMockOptionalButtonView).setEnabled(false);
        verify(mMockOptionalButtonView, times(2)).updateButtonWithAnimation(buttonData);
    }

    @Test
    public void testShowIphAfterButtonUpdateTransition() {
        verify(mMockOptionalButtonView)
                .setTransitionFinishedCallback(mCallbackArgumentCaptor.capture());
        Callback<Integer> transitionFinishedCallback = mCallbackArgumentCaptor.getValue();

        OnClickListener clickListener = ViewUtils.emptyClickListener();
        OnLongClickListener longClickListener = ViewUtils.emptyLongClickListener();
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(mDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setOnLongClickListener(longClickListener)
                        .setIphCommandBuilder(mIphCommandBuilder)
                        .setHasErrorBadge(false)
                        .build();
        ButtonDataImpl buttonData = new ButtonDataImpl(/* canShow= */ false, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        // Call the finished callback twice to ensure the IPH is only shown once.
        transitionFinishedCallback.onResult(TransitionType.SWAPPING);
        transitionFinishedCallback.onResult(TransitionType.SWAPPING);

        // IPH should have been built and shown only once.
        verify(mIphCommandBuilder).build();
        verify(mMockUserEducationHelper).requestShowIph(any());
    }
}
