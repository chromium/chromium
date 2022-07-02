// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable.Bounds;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The coordinator for a button that may appear on the toolbar whose icon and click handler can be
 * updated with animations.
 */
public class OptionalButtonCoordinator {
    private final OptionalButtonMediator mMediator;
    private final OptionalButtonView mView;
    private final UserEducationHelper mUserEducationHelper;
    private Callback<Integer> mTransitionFinishedCallback;
    private IPHCommandBuilder mIphCommandBuilder;

    @IntDef({TransitionType.SWAPPING, TransitionType.SHOWING, TransitionType.HIDING,
            TransitionType.EXPANDING_ACTION_CHIP, TransitionType.COLLAPSING_ACTION_CHIP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TransitionType {
        int SWAPPING = 0;
        int SHOWING = 1;
        int HIDING = 2;
        int EXPANDING_ACTION_CHIP = 3;
        int COLLAPSING_ACTION_CHIP = 4;
    }

    public OptionalButtonCoordinator(View view, UserEducationHelper userEducationHelper) {
        mUserEducationHelper = userEducationHelper;
        PropertyModel model = new PropertyModel.Builder(OptionalButtonProperties.ALL_KEYS)
                                      .with(OptionalButtonProperties.TRANSITION_FINISHED_CALLBACK,
                                              this::onTransitionFinishedCallback)
                                      .build();

        assert view instanceof OptionalButtonView;

        this.mView = (OptionalButtonView) view;

        PropertyModelChangeProcessor.create(model, mView, OptionalButtonViewBinder::bind);

        mMediator = new OptionalButtonMediator(model);
    }

    public void setPaddingStart(int paddingStart) {
        mMediator.setPaddingStart(paddingStart);
    }

    /**
     * Sets the ViewGroup that contains all the views that will be affected by our transitions. It
     * has to be the parent view because the action chip width changes affect our sibling views.
     * @param transitionRoot
     */
    public void setTransitionRoot(ViewGroup transitionRoot) {
        mMediator.setTransitionRoot(transitionRoot);
    }

    public void setOnBeforeHideTransitionCallback(Runnable onBeforeHideTransitionCallback) {
        mMediator.setOnBeforeHideTransitionCallback(onBeforeHideTransitionCallback);
    }

    /**
     * Sets a callable that returns a boolean, this is called before any icon updates to ensure that
     * the containing view is in a state that allows animations. If animations aren't allowed then
     * the icon update is done instantly without any animation. Even if animations are not allowed
     * all callbacks are called instantly in order.
     * @param isAnimationAllowedPredicate
     */
    public void setIsAnimationAllowedPredicate(BooleanSupplier isAnimationAllowedPredicate) {
        mMediator.setIsAnimationAllowedPredicate(isAnimationAllowedPredicate);
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
     * @param buttonData
     */
    public void updateButton(ButtonData buttonData) {
        if (buttonData != null && buttonData.getButtonSpec() != null
                && buttonData.getButtonSpec().getIPHCommandBuilder() != null) {
            mIphCommandBuilder = buttonData.getButtonSpec().getIPHCommandBuilder();
            setViewSpecificIphProperties(mIphCommandBuilder);
        } else {
            mIphCommandBuilder = null;
        }

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
     * @param colorStateList
     */
    public void setIconForegroundColor(ColorStateList colorStateList) {
        mMediator.setIconForegroundColor(colorStateList);
    }

    /**
     * Updates the color filter of the background to match the current theme/website color.
     * @param backgroundColor
     */
    public void setBackgroundColorFilter(int backgroundColor) {
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
    @VisibleForTesting
    public View getButtonViewForTesting() {
        return mView.getButtonView();
    }

    private void onTransitionFinishedCallback(@TransitionType int transitionType) {
        if (mTransitionFinishedCallback != null) {
            mTransitionFinishedCallback.onResult(transitionType);
        }

        if (mIphCommandBuilder != null) {
            mUserEducationHelper.requestShowIPH(mIphCommandBuilder.build());
            mIphCommandBuilder = null;
        }
    }

    private void setViewSpecificIphProperties(IPHCommandBuilder iphCommandBuilder) {
        HighlightParams highlightParams = new HighlightParams(HighlightShape.CIRCLE);
        highlightParams.setCircleRadius(new Bounds() {
            @Override
            public float getMaxRadiusPx(Rect bounds) {
                return mView.getResources().getDisplayMetrics().density * 20;
            }

            @Override
            public float getMinRadiusPx(Rect bounds) {
                return mView.getResources().getDisplayMetrics().density * 20;
            }
        });

        ViewRectProvider viewRectProvider = new ViewRectProvider(mView);
        viewRectProvider.setIncludePadding(false);

        highlightParams.setBoundsRespectPadding(false);
        iphCommandBuilder.setAnchorView(
                mView.getButtonView() == null ? mView : mView.getButtonView());
        iphCommandBuilder.setViewRectProvider(viewRectProvider);
        iphCommandBuilder.setHighlightParams(highlightParams);
    }
}
