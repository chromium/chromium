// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import static junit.framework.Assert.assertEquals;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.transition.Transition;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator.TransitionType;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.util.function.BooleanSupplier;
/**
 * Unit tests for OptionalButtonCoordinator.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OptionalButtonCoordinatorTest {
    @Mock
    private ViewGroup mMockRootView;
    @Mock
    private BooleanSupplier mMockIsAnimationAllowedDelegate;
    @Mock
    private OptionalButtonView mMockOptionalButtonView;
    @Mock
    private UserEducationHelper mMockUserEducationHelper;
    @Mock
    private Callback<Transition> mMockBeginDelayedTransition;
    @Mock
    private Tracker mMockTracker;

    @Captor
    ArgumentCaptor<Callback<Integer>> mCallbackArgumentCaptor;

    OptionalButtonCoordinator mOptionalButtonCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mOptionalButtonCoordinator =
                new OptionalButtonCoordinator(mMockOptionalButtonView, mMockUserEducationHelper,
                        mMockRootView, mMockIsAnimationAllowedDelegate, mMockTracker);
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

        Callback<Integer> externalCallback = Mockito.mock(Callback.class);

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
    public void testSetIconForegroundColor() {
        ColorStateList colorStateList = ColorStateList.valueOf(Color.RED);

        mOptionalButtonCoordinator.setIconForegroundColor(colorStateList);

        verify(mMockOptionalButtonView).setColorStateList(colorStateList);
    }

    @Test
    public void testSetBackgroundColorFilter() {
        mOptionalButtonCoordinator.setBackgroundColorFilter(Color.GREEN);

        verify(mMockOptionalButtonView).setBackgroundColorFilter(Color.GREEN);
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
        View mockView = mock(View.class);
        when(mMockOptionalButtonView.getButtonView()).thenReturn(mockView);

        assertEquals(mockView, mOptionalButtonCoordinator.getButtonViewForTesting());

        verify(mMockOptionalButtonView).getButtonView();
    }

    @Test
    public void testUpdateButton() {
        Drawable iconDrawable = mock(Drawable.class);
        OnClickListener clickListener = view -> {};
        IPHCommandBuilder mockIphCommandBuilder = mock(IPHCommandBuilder.class);
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, iconDrawable, clickListener,
                contentDescription, /* supportsTinting= */ true, mockIphCommandBuilder,
                /* isEnabled= */ isEnabled, AdaptiveToolbarButtonVariant.UNKNOWN);

        mOptionalButtonCoordinator.updateButton(buttonData);

        // IPH command builder must be populated with view specific properties.
        verify(mockIphCommandBuilder).setAnchorView(any());
        verify(mockIphCommandBuilder).setViewRectProvider(any());
        verify(mockIphCommandBuilder).setHighlightParams(any());
        verifyNoMoreInteractions(mockIphCommandBuilder);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsRemovedWhenNotInVariant() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, "action_chip", "false");
        FeatureList.setTestValues(testValues);

        Drawable iconDrawable = mock(Drawable.class);
        OnClickListener clickListener = view -> {};
        IPHCommandBuilder mockIphCommandBuilder = mock(IPHCommandBuilder.class);
        String contentDescription = "description";
        int actionChipResourceId = 987654;
        boolean isEnabled = true;
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, iconDrawable, clickListener,
                contentDescription, actionChipResourceId, /* supportsTinting= */ true,
                mockIphCommandBuilder,
                /* isEnabled= */ isEnabled, AdaptiveToolbarButtonVariant.PRICE_TRACKING);

        mOptionalButtonCoordinator.updateButton(buttonData);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
        Assert.assertEquals(
                Resources.ID_NULL, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsRemovedByFeatureEngagement() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, "action_chip", "true");
        FeatureList.setTestValues(testValues);

        doReturn(true).when(mMockTracker).isInitialized();
        doReturn(false)
                .when(mMockTracker)
                .shouldTriggerHelpUI(FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP);

        Drawable iconDrawable = mock(Drawable.class);
        OnClickListener clickListener = view -> {};
        IPHCommandBuilder mockIphCommandBuilder = mock(IPHCommandBuilder.class);
        String contentDescription = "description";
        int actionChipResourceId = 987654;
        boolean isEnabled = true;
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, iconDrawable, clickListener,
                contentDescription, actionChipResourceId, /* supportsTinting= */ true,
                mockIphCommandBuilder,
                /* isEnabled= */ isEnabled, AdaptiveToolbarButtonVariant.PRICE_TRACKING);

        mOptionalButtonCoordinator.updateButton(buttonData);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
        Assert.assertEquals(
                Resources.ID_NULL, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_actionChipResourceIdGetsKeptByFeatureEngagement() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, "action_chip", "true");
        FeatureList.setTestValues(testValues);

        doReturn(true).when(mMockTracker).isInitialized();
        doReturn(true)
                .when(mMockTracker)
                .shouldTriggerHelpUI(FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP);

        Drawable iconDrawable = mock(Drawable.class);
        OnClickListener clickListener = view -> {};
        IPHCommandBuilder mockIphCommandBuilder = mock(IPHCommandBuilder.class);
        String contentDescription = "description";
        int actionChipResourceId = 987654;
        boolean isEnabled = true;
        ButtonData buttonData = new ButtonDataImpl(/* canShow= */ true, iconDrawable, clickListener,
                contentDescription, actionChipResourceId, /* supportsTinting= */ true,
                mockIphCommandBuilder,
                /* isEnabled= */ isEnabled, AdaptiveToolbarButtonVariant.PRICE_TRACKING);

        mOptionalButtonCoordinator.updateButton(buttonData);

        verify(mMockOptionalButtonView).updateButtonWithAnimation(buttonData);
        Assert.assertEquals(
                actionChipResourceId, buttonData.getButtonSpec().getActionChipLabelResId());
    }

    @Test
    public void testUpdateButton_disableButtonWithoutChanges() {
        View mockButtonView = mock(View.class);
        when(mMockOptionalButtonView.getButtonView()).thenReturn(mockButtonView);

        Drawable iconDrawable = mock(Drawable.class);
        OnClickListener clickListener = view -> {};
        String contentDescription = "description";
        ButtonDataImpl buttonData = new ButtonDataImpl(/* canShow= */ true, iconDrawable,
                clickListener, contentDescription, /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                /* isEnabled= */ true, AdaptiveToolbarButtonVariant.UNKNOWN);

        // Call update button with an enabled button.
        mOptionalButtonCoordinator.updateButton(buttonData);

        buttonData.setEnabled(false);

        // Call updateButton with the same data, but with enabled = false.
        mOptionalButtonCoordinator.updateButton(buttonData);

        // Button should be disabled.
        verify(mockButtonView).setEnabled(false);
        verify(mMockOptionalButtonView, times(2)).updateButtonWithAnimation(buttonData);
    }

    @Test
    public void testShowIphAfterButtonUpdateTransition() {
        verify(mMockOptionalButtonView)
                .setTransitionFinishedCallback(mCallbackArgumentCaptor.capture());
        Callback<Integer> transitionFinishedCallback = mCallbackArgumentCaptor.getValue();

        Drawable iconDrawable = mock(Drawable.class);
        OnClickListener clickListener = view -> {};
        OnLongClickListener longClickListener = view -> {
            return false;
        };
        IPHCommandBuilder mockIphCommandBuilder = mock(IPHCommandBuilder.class);
        String contentDescription = "description";
        boolean isEnabled = true;
        ButtonSpec buttonSpec = new ButtonSpec(iconDrawable, clickListener, longClickListener,
                contentDescription, true, mockIphCommandBuilder,
                AdaptiveToolbarButtonVariant.UNKNOWN, /*actionChipLabelResId=*/0);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setEnabled(isEnabled);

        mOptionalButtonCoordinator.updateButton(buttonData);

        // Call the finished callback twice to ensure the IPH is only shown once.
        transitionFinishedCallback.onResult(TransitionType.SWAPPING);
        transitionFinishedCallback.onResult(TransitionType.SWAPPING);

        // IPH should have been built and shown only once.
        verify(mockIphCommandBuilder).build();
        verify(mMockUserEducationHelper).requestShowIPH(any());
    }
}
