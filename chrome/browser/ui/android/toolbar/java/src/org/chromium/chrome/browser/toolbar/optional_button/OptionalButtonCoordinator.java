// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable.Bounds;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;

/**
 * The coordinator for a button that may appear on the toolbar whose icon and click handler can be
 * updated with animations.
 */
public class OptionalButtonCoordinator {
    private final OptionalButtonMediator mMediator;
    private final OptionalButtonView mView;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<Tracker> mFeatureEngagementTrackerSupplier;
    private Callback<Integer> mTransitionFinishedCallback;
    private IPHCommandBuilder mIphCommandBuilder;

    @IntDef({
        TransitionType.SWAPPING,
        TransitionType.SHOWING,
        TransitionType.HIDING,
        TransitionType.EXPANDING_ACTION_CHIP,
        TransitionType.COLLAPSING_ACTION_CHIP
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TransitionType {
        int SWAPPING = 0;
        int SHOWING = 1;
        int HIDING = 2;
        int EXPANDING_ACTION_CHIP = 3;
        int COLLAPSING_ACTION_CHIP = 4;
    }

    /**
     * Creates a new instance of OptionalButtonCoordinator
     *
     * @param view An instance of OptionalButtonView to bind to.
     * @param userEducationHelper Used to display highlight the button with IPH if needed.
     * @param transitionRoot ViewGroup that contains all the views that will be affected by our
     *     transitions.
     * @param isAnimationAllowedPredicate A BooleanProvider that is called before all transitions to
     *     determine if said transition should be animated or not.
     * @param featureEngagementTrackerSupplier Provides a {@Tracker} when available.
     */
    public OptionalButtonCoordinator(
            View view,
            UserEducationHelper userEducationHelper,
            ViewGroup transitionRoot,
            BooleanSupplier isAnimationAllowedPredicate,
            Supplier<Tracker> featureEngagementTrackerSupplier) {
        mUserEducationHelper = userEducationHelper;
        PropertyModel model =
                new PropertyModel.Builder(OptionalButtonProperties.ALL_KEYS)
                        .with(
                                OptionalButtonProperties.TRANSITION_FINISHED_CALLBACK,
                                this::onTransitionFinishedCallback)
                        .with(OptionalButtonProperties.TRANSITION_ROOT, transitionRoot)
                        .with(
                                OptionalButtonProperties.IS_ANIMATION_ALLOWED_PREDICATE,
                                isAnimationAllowedPredicate)
                        .build();

        assert view instanceof OptionalButtonView;

        mView = (OptionalButtonView) view;

        PropertyModelChangeProcessor.create(model, mView, OptionalButtonViewBinder::bind);

        mMediator = new OptionalButtonMediator(model);
        mFeatureEngagementTrackerSupplier = featureEngagementTrackerSupplier;
    }

    public void setPaddingStart(int paddingStart) {
        mMediator.setPaddingStart(paddingStart);
    }

    public void setOnBeforeHideTransitionCallback(Runnable onBeforeHideTransitionCallback) {
        mMediator.setOnBeforeHideTransitionCallback(onBeforeHideTransitionCallback);
    }

    /**
     * Sets a callback that's invoked when any transition starts.
     * @param transitionStartedCallback A callback with an integer argument, this argument a value
     *         from {@link TransitionType}.
     */
    public void setTransitionStartedCallback(Callback<Integer> transitionStartedCallback) {
        mMediator.setTransitionStartedCallback(transitionStartedCallback);
    }

    /**
     * Sets a callback that's invoked when any transition is finished.
     * @param transitionFinishedCallback A callback with an integer argument, this argument a value
     *         from {@link TransitionType}.
     */
    public void setTransitionFinishedCallback(Callback<Integer> transitionFinishedCallback) {
        mTransitionFinishedCallback = transitionFinishedCallback;
    }

    /**
     * Updates the button to replace the current action with a new one. If animations are allowed
     * (according to the BooleanSupplier set with setIsAnimationAllowedPredicate) then this update
     * will be animated. Otherwise it'll instantly switch to the new icon.
     */
    public void updateButton(ButtonData buttonData) {
        if (buttonData != null
                && buttonData.getButtonSpec() != null
                && buttonData.getButtonSpec().getIPHCommandBuilder() != null) {
            mIphCommandBuilder = buttonData.getButtonSpec().getIPHCommandBuilder();
            setViewSpecificIphProperties(mIphCommandBuilder);
        } else {
            mIphCommandBuilder = null;
        }

        boolean hasActionChipResourceId =
                buttonData != null
                        && buttonData.getButtonSpec().getActionChipLabelResId()
                                != Resources.ID_NULL;

        // Dynamic buttons include an action chip resource ID by default regardless of variant.
        if (hasActionChipResourceId) {
            // We should only show the action chip if the action chip variant is enabled.
            boolean isActionChipVariant =
                    FeatureList.isInitialized()
                            && AdaptiveToolbarFeatures.shouldShowActionChip(
                                    buttonData.getButtonSpec().getButtonVariant());
            // And if feature engagement allows it.
            Tracker featureEngagementTracker = mFeatureEngagementTrackerSupplier.get();
            boolean shouldShowActionChip =
                    isActionChipVariant
                            && featureEngagementTracker != null
                            && featureEngagementTracker.isInitialized()
                            && featureEngagementTracker.shouldTriggerHelpUI(
                                    FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP);

            if (!shouldShowActionChip) {
                ((ButtonDataImpl) buttonData).updateActionChipResourceId(Resources.ID_NULL);
            }
        }

        // Reset background alpha, in case the IPH onDismiss callback doesn't fire.
        mMediator.setBackgroundAlpha(255);
        mMediator.updateButton(buttonData);
    }

    /**
     * Updates the button to hide it. If animations are allowed (according to the BooleanSupplier
     * set with setIsAnimationAllowedPredicate) then this update will be animated. Otherwise it'll
     * hide instantly.
     */
    public void hideButton() {
        mIphCommandBuilder = null;

        mMediator.updateButton(null);
    }

    /**
     * If there's any transition animation it gets canceled and we fast forward to the next visual
     * state. The TransitionFinished callback is invoked.
     */
    public void cancelTransition() {
        mMediator.cancelTransition();
    }

    /**
     * Updates the foreground color on the icons and label to match the current theme/website color.
     */
    public void setIconForegroundColor(ColorStateList colorStateList) {
        mMediator.setIconForegroundColor(colorStateList);
    }

    /**
     * Updates the color filter of the background to match the current address bar background color.
     * This color is only used when showing a contextual action button (when {@link
     * #updateButton(ButtonData)} is called with a {@link
     * org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec} where {@code isDynamicAction()} is
     * true).
     */
    public void setBackgroundColorFilter(@ColorInt int backgroundColor) {
        mMediator.setBackgroundColorFilter(backgroundColor);
    }

    public int getViewVisibility() {
        return mView.getVisibility();
    }

    /**
     * Gets the current width of the container view, used by ToolbarPhone for laying out other
     * views.
     */
    public int getViewWidth() {
        return mView.getWidth();
    }

    /**
     * Gets the container for the button, meant to be used by ToolbarPhone for drawing this view
     * into a texture.
     */
    public View getViewForDrawing() {
        return mView;
    }

    /** Gets the underlying ButtonView. */
    public View getButtonViewForTesting() {
        return mView.getButtonView();
    }

    private void onTransitionFinishedCallback(@TransitionType int transitionType) {
        if (mTransitionFinishedCallback != null) {
            mTransitionFinishedCallback.onResult(transitionType);
        }

        if (transitionType == TransitionType.EXPANDING_ACTION_CHIP
                && mFeatureEngagementTrackerSupplier.hasValue()) {
            // Record an event in feature engagement to limit the amount of times we show the action
            // chip.
            Tracker featureEngagementTracker = mFeatureEngagementTrackerSupplier.get();
            featureEngagementTracker.addOnInitializedCallback(
                    isReady -> {
                        if (!isReady) return;
                        featureEngagementTracker.dismissed(
                                FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_ACTION_CHIP);
                    });
        }

        if (mIphCommandBuilder != null) {
            mUserEducationHelper.requestShowIPH(mIphCommandBuilder.build());
            mIphCommandBuilder = null;
        }
    }

    private void setViewSpecificIphProperties(IPHCommandBuilder iphCommandBuilder) {
        HighlightParams highlightParams = new HighlightParams(HighlightShape.CIRCLE);
        highlightParams.setCircleRadius(
                new Bounds() {
                    @Override
                    public float getMaxRadiusPx(Rect bounds) {
                        return mView.getResources().getDisplayMetrics().density * 20;
                    }

                    @Override
                    public float getMinRadiusPx(Rect bounds) {
                        return mView.getResources().getDisplayMetrics().density * 20;
                    }
                });

        // We want this IPH highlight to be on the same position as the button's background which is
        // an ImageView separate from the button's ListMenuButton. IPH highlights are implemented as
        // a drawable set to the view's background (something like:
        // backgroundImageView.setBackground(drawable)). If we try to highlight the background's
        // ImageView nothing will be shown, because the highlight is obstructed by the image. Set
        // callbacks to make the background image transparent so the highlight is visible. This gets
        // reset once the IPH is dismissed.
        iphCommandBuilder.setOnShowCallback(() -> mMediator.setBackgroundAlpha(0));
        iphCommandBuilder.setOnDismissCallback(() -> mMediator.setBackgroundAlpha(255));

        View anchorView = mView;
        View backgroundView = mView.getBackgroundView();
        if (backgroundView != null && backgroundView.getVisibility() != View.GONE) {
            anchorView = backgroundView;
        }
        ViewRectProvider viewRectProvider = new ViewRectProvider(anchorView);
        viewRectProvider.setIncludePadding(false);

        highlightParams.setBoundsRespectPadding(true);
        iphCommandBuilder.setAnchorView(anchorView);
        iphCommandBuilder.setViewRectProvider(viewRectProvider);
        iphCommandBuilder.setHighlightParams(highlightParams);
    }
}
