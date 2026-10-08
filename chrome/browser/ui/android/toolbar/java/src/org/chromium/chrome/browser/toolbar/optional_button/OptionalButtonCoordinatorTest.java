// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
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
    @Mock private BooleanSupplier mMockIsAnimationAllowedDelegate;
    @Mock private UserEducationHelper mMockUserEducationHelper;
    @Mock private Tracker mMockTracker;

    @Captor ArgumentCaptor<ViewRectProvider> mViewRectProviderCaptor;

    private ViewGroup mRootView;
    private OptionalButtonView mOptionalButtonView;
    OptionalButtonCoordinator mOptionalButtonCoordinator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRootView = new FrameLayout(activity);
        activity.setContentView(mRootView);
        mOptionalButtonView =
                (OptionalButtonView)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.optional_button_layout,
                                        mRootView,
                                        /* attachToRoot= */ false);
        mRootView.addView(mOptionalButtonView);
        mRootView.layout(0, 0, 100, 100);
        mOptionalButtonView.layout(0, 0, 100, 100);

        mOptionalButtonCoordinator =
                new OptionalButtonCoordinator(
                        mOptionalButtonView,
                        () -> mMockUserEducationHelper,
                        mRootView,
                        mMockIsAnimationAllowedDelegate,
                        ObservableSuppliers.createNonNull(mMockTracker));
    }

    @Test
    public void testSetOnBeforeHideTransitionCallback() {
        Runnable callback = mock(Runnable.class);

        mOptionalButtonCoordinator.setOnBeforeHideTransitionCallback(callback);
        mOptionalButtonCoordinator.hideButton();

        verify(callback).run();
    }

    @Test
    public void testSetTransitionStartedCallback() {
        Callback<Integer> callback = MockitoHelper.mockCallback();

        mOptionalButtonCoordinator.setTransitionStartedCallback(callback);
        mOptionalButtonCoordinator.hideButton();

        verify(callback).onResult(TransitionType.HIDING);
    }

    @Test
    public void testSetTransitionFinishedCallback() {
        Callback<Integer> externalCallback = MockitoHelper.mockCallback();

        // Set a callback.
        mOptionalButtonCoordinator.setTransitionFinishedCallback(externalCallback);
        mOptionalButtonCoordinator.hideButton();

        // Check that the external callback is invoked when the transition finishes.
        verify(externalCallback).onResult(TransitionType.HIDING);
    }

    @Test
    public void testSetBackgroundColorFilter() {
        ImageView background = (ImageView) mOptionalButtonView.getBackgroundView();
        mOptionalButtonCoordinator.setBackgroundColorFilter(Color.GREEN);

        assertEquals(
                new PorterDuffColorFilter(Color.GREEN, PorterDuff.Mode.SRC_ATOP),
                background.getColorFilter());

        mOptionalButtonCoordinator.setBackgroundColorFilter(Color.RED);

        assertEquals(
                new PorterDuffColorFilter(Color.RED, PorterDuff.Mode.SRC_ATOP),
                background.getColorFilter());
    }

    @Test
    public void testSetBrandedColorScheme() {
        TextView actionChipLabel = mOptionalButtonView.findViewById(R.id.action_chip_label);
        mOptionalButtonCoordinator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        mOptionalButtonView.getContext(), BrandedColorScheme.LIGHT_BRANDED_THEME),
                actionChipLabel.getTextColors());

        mOptionalButtonCoordinator.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        mOptionalButtonView.getContext(), BrandedColorScheme.DARK_BRANDED_THEME),
                actionChipLabel.getTextColors());
    }

    @Test
    public void testSetPaddingStart() {
        assertEquals(0, mOptionalButtonView.getPaddingStart());
        mOptionalButtonCoordinator.setPaddingStart(42);

        assertEquals(42, mOptionalButtonView.getPaddingStart());
    }

    @Test
    public void testCancelTransition() {
        when(mMockIsAnimationAllowedDelegate.getAsBoolean()).thenReturn(true);
        Callback<Integer> finishedCallback = MockitoHelper.mockCallback();
        mOptionalButtonCoordinator.setTransitionFinishedCallback(finishedCallback);

        mOptionalButtonCoordinator.hideButton();
        mOptionalButtonCoordinator.cancelTransition();
        verify(finishedCallback).onResult(TransitionType.HIDING);

        mOptionalButtonCoordinator.hideButton();
        mOptionalButtonCoordinator.cancelTransition();
        verify(finishedCallback, times(2)).onResult(TransitionType.HIDING);
    }

    @Test
    public void testGetViewVisibility() {
        mOptionalButtonView.setVisibility(View.VISIBLE);
        assertEquals(View.VISIBLE, mOptionalButtonCoordinator.getViewVisibility());

        mOptionalButtonView.setVisibility(View.GONE);
        assertEquals(View.GONE, mOptionalButtonCoordinator.getViewVisibility());
    }

    @Test
    public void testGetViewWidth() {
        mOptionalButtonView.layout(0, 0, 100, 50);

        assertEquals(100, mOptionalButtonCoordinator.getViewWidth());
    }

    @Test
    public void testGetViewForDrawing() {
        assertEquals(mOptionalButtonView, mOptionalButtonCoordinator.getViewForDrawing());
    }

    @Test
    public void testGetButtonView() {
        assertEquals(
                mOptionalButtonView.findViewById(R.id.optional_toolbar_button),
                mOptionalButtonCoordinator.getButtonView());
    }

    @Test
    public void testUpdateButton_hasErrorBadge() {
        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        OnLongClickListener longClickListener = ViewUtils.emptyLongClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setOnLongClickListener(longClickListener)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .setHasErrorBadge(true)
                        .build();
        ButtonDataImpl buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        assertEquals(
                mOptionalButtonView
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen
                                        .optional_toolbar_phone_button_with_error_badge_padding_bottom),
                mOptionalButtonView.getButtonView().getPaddingBottom());
    }

    @Test
    public void testUpdateButton_backgroundVisible() {
        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        View backgroundView = mOptionalButtonView.getBackgroundView();
        backgroundView.setVisibility(View.VISIBLE);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        // IPH command builder must be populated with view specific properties.
        verify(mockIphCommandBuilder).setAnchorView(eq(backgroundView));
        verify(mockIphCommandBuilder).setViewRectProvider(mViewRectProviderCaptor.capture());
        assertEquals(backgroundView, mViewRectProviderCaptor.getValue().getViewForTesting());
        verify(mockIphCommandBuilder).setHighlightParams(any());
        verify(mockIphCommandBuilder).setOnShowCallback(any());
        verify(mockIphCommandBuilder).setOnDismissCallback(any());
        verify(mockIphCommandBuilder).build();
        verifyNoMoreInteractions(mockIphCommandBuilder);

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
    }

    @Test
    public void testUpdateButton_backgroundGone() {
        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        View backgroundView = mOptionalButtonView.getBackgroundView();
        backgroundView.setVisibility(View.GONE);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        // IPH command builder must be populated with view specific properties.
        verify(mockIphCommandBuilder).setAnchorView(eq(mOptionalButtonView));
        verify(mockIphCommandBuilder).setViewRectProvider(mViewRectProviderCaptor.capture());
        assertEquals(mOptionalButtonView, mViewRectProviderCaptor.getValue().getViewForTesting());
        verify(mockIphCommandBuilder).setHighlightParams(any());
        verify(mockIphCommandBuilder).setOnShowCallback(any());
        verify(mockIphCommandBuilder).setOnDismissCallback(any());
        verify(mockIphCommandBuilder).build();
        verifyNoMoreInteractions(mockIphCommandBuilder);

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
    }

    @Test
    public void testUpdateButton_showingIphChangesBackgroundAlpha() {
        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        ArgumentCaptor<Runnable> onShowCallbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        ArgumentCaptor<Runnable> onDismissCallbackCaptor = ArgumentCaptor.forClass(Runnable.class);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        verify(mockIphCommandBuilder).setOnShowCallback(onShowCallbackCaptor.capture());
        verify(mockIphCommandBuilder).setOnDismissCallback(onDismissCallbackCaptor.capture());

        ImageView background = (ImageView) mOptionalButtonView.getBackgroundView();

        // Showing an IPH should make the background transparent to be able to see the highlight.
        onShowCallbackCaptor.getValue().run();
        assertEquals(0, background.getImageAlpha());

        // Dismissing the IPH should bring back the background to normal.
        onDismissCallbackCaptor.getValue().run();
        assertEquals(255, background.getImageAlpha());
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsRemovedWhenNotInVariant() {
        AdaptiveToolbarFeatures.setIsDynamicActionForTesting(
                AdaptiveToolbarButtonVariant.TEST_BUTTON, true);
        FeatureOverrides.overrideParam(
                AdaptiveToolbarFeatures.CONTEXTUAL_PAGE_ACTION_TEST_FEATURE_NAME,
                "action_chip",
                false);

        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        int actionChipResourceId = R.string.actionbar_share;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.TEST_BUTTON)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
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

        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        int actionChipResourceId = R.string.actionbar_share;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.TEST_BUTTON)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
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

        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        int actionChipResourceId = R.string.actionbar_share;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.TEST_BUTTON)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
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

        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        int actionChipResourceId = R.string.actionbar_share;
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setActionChipLabelResId(actionChipResourceId)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.GLIC)
                        .setHoverTooltipTextId(Resources.ID_NULL)
                        .build();
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        assertEquals(View.VISIBLE, mOptionalButtonView.getVisibility());
        assertEquals(actionChipResourceId, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_disableButtonWithoutChanges() {
        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        String contentDescription = "description";
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .build();
        ButtonDataImpl buttonData =
                new ButtonDataImpl(/* canShow= */ true, /* isEnabled= */ true, buttonSpec);

        // Call update button with an enabled button.
        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);
        assertTrue(mOptionalButtonCoordinator.getButtonView().isEnabled());

        buttonData.setEnabled(false);

        // Call updateButton with the same data, but with enabled = false.
        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);

        // Button should be disabled.
        assertFalse(mOptionalButtonCoordinator.getButtonView().isEnabled());
    }

    @Test
    public void testShowIphAfterButtonUpdateTransition() {
        Callback<Integer> transitionFinishedCallback = MockitoHelper.mockCallback();
        mOptionalButtonCoordinator.setTransitionFinishedCallback(transitionFinishedCallback);

        Drawable iconDrawable = new ColorDrawable(Color.RED);
        OnClickListener clickListener = ViewUtils.emptyClickListener();
        OnLongClickListener longClickListener = ViewUtils.emptyLongClickListener();
        IphCommandBuilder mockIphCommandBuilder = mock(IphCommandBuilder.class);
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec =
                new ButtonSpec.Builder(
                                iconDrawable, contentDescription, /* supportsTinting= */ true)
                        .setOnClickListener(clickListener)
                        .setOnLongClickListener(longClickListener)
                        .setIphCommandBuilder(mockIphCommandBuilder)
                        .setHasErrorBadge(false)
                        .build();
        ButtonDataImpl buttonData = new ButtonDataImpl(/* canShow= */ false, isEnabled, buttonSpec);

        mOptionalButtonCoordinator.updateButton(buttonData, /* isIncognito= */ false);
        // Trigger a second transition without calling OptionalButtonCoordinator#updateButton to
        // ensure the IPH is only shown once across multiple transition completions.
        mOptionalButtonView.updateButtonWithAnimation(buttonData);

        verify(transitionFinishedCallback, times(2)).onResult(TransitionType.HIDING);

        // IPH should have been built and shown only once.
        verify(mockIphCommandBuilder).build();
        verify(mMockUserEducationHelper).requestShowIph(any());
    }
}
