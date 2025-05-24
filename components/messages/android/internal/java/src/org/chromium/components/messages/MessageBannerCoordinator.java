// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;
import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityEventCompat;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.messages.MessageFeatureMap.AccessibilityEventInvestigationGroup;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.listmenu.ListMenuHost.PopupMenuShownListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.RunnableTimer;

/** Coordinator responsible for creating a message banner. */
@NullMarked
class MessageBannerCoordinator {
    private final MessageBannerMediator mMediator;
    private final MessageBannerView mView;
    private final View mParentView;
    private final PropertyModel mModel;
    private final RunnableTimer mTimer;
    private final Supplier<Long> mAutodismissDurationMs;
    private final Runnable mOnTimeUp;

    /**
     * Constructs the message banner.
     *
     * @param view The inflated {@link MessageBannerView}.
     * @param model The model for the message banner.
     * @param maxTranslationSupplier A {@link Supplier} that supplies the maximum translation Y
     *     value the message banner can have as a result of the animations or the gestures.
     * @param topOffsetSupplier A {@link Supplier} that supplies the message's top offset.
     * @param resources The {@link Resources}.
     * @param parentView The parent {@link View}.
     * @param messageDismissed The {@link Runnable} that will run if and when the user dismisses the
     *     message.
     * @param swipeAnimationHandler The handler that will be used to delegate starting the
     *     animations to {@link WindowAndroid} so the message is not clipped as a result of some
     *     Android SurfaceView optimization.
     * @param autodismissDurationMs A {@link Supplier} providing autodismiss duration for message
     *     banner.
     * @param onTimeUp A {@link Runnable} that will run if and when the auto dismiss timer is up.
     */
    MessageBannerCoordinator(
            MessageBannerView view,
            PropertyModel model,
            Supplier<Integer> maxTranslationSupplier,
            Supplier<Integer> topOffsetSupplier,
            Resources resources,
            View parentView,
            Runnable messageDismissed,
            SwipeAnimationHandler swipeAnimationHandler,
            Supplier<Long> autodismissDurationMs,
            Runnable onTimeUp) {
        mView = view;
        mParentView = parentView;
        mModel = model;
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        mMediator =
                new MessageBannerMediator(
                        model,
                        topOffsetSupplier,
                        maxTranslationSupplier,
                        resources,
                        messageDismissed,
                        swipeAnimationHandler);
        mAutodismissDurationMs = autodismissDurationMs;
        mTimer = new RunnableTimer();
        mOnTimeUp = onTimeUp;
        view.setSwipeHandler(mMediator);
        view.setPopupMenuShownListener(
                createPopupMenuShownListener(mTimer, mAutodismissDurationMs.get(), mOnTimeUp));
    }

    /**
     * Creates a {@link PopupMenuShownListener} to handle secondary button popup menu events on the
     * message banner.
     *
     * @param timer The {@link RunnableTimer} controlling the message banner dismiss duration.
     * @param duration The auto dismiss duration for the message banner.
     * @param onTimeUp A {@link Runnable} that will run if and when the auto dismiss timer is up.
     */
    @VisibleForTesting
    PopupMenuShownListener createPopupMenuShownListener(
            RunnableTimer timer, long duration, Runnable onTimeUp) {
        return new PopupMenuShownListener() {
            @Override
            public void onPopupMenuShown() {
                timer.cancelTimer();
            }

            @Override
            public void onPopupMenuDismissed() {
                timer.startTimer(duration, onTimeUp);
            }
        };
    }

    /**
     * Shows the message banner.
     * @param fromIndex The initial position.
     * @param toIndex The target position the message is moving to.
     * @param messageDimensSupplier Supplier of dimensions of the message next to current one.
     * @return The animator which shows the message view.
     */
    Animator show(
            @Position int fromIndex,
            @Position int toIndex,
            Supplier<MessageDimens> messageDimensSupplier) {
        mView.dismissSecondaryMenuIfShown();
        int verticalOffset = 0;
        if (toIndex == Position.BACK) {
            MessageDimens prevMessageDimens = messageDimensSupplier.get();
            int height = mView.getHeight();
            if (!mView.isLaidOut()) {
                int maxWidth = prevMessageDimens.getWidth();
                int wSpec = View.MeasureSpec.makeMeasureSpec(maxWidth, View.MeasureSpec.AT_MOST);
                int hSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
                mView.measure(wSpec, hSpec);
                height = mView.getMeasuredHeight();
            }
            if (height < prevMessageDimens.getHeight()) {
                verticalOffset = prevMessageDimens.getHeight() - height;
            } else if (height > prevMessageDimens.getHeight()) {
                mView.resizeForStackingAnimation(
                        prevMessageDimens.getTitleHeight(),
                        prevMessageDimens.getDescriptionHeight(),
                        prevMessageDimens.getPrimaryButtonLineCount());
            }
        } else if (fromIndex == Position.BACK && toIndex == Position.FRONT) {
            mView.resetForStackingAnimation();
        }
        return mMediator.show(
                fromIndex,
                toIndex,
                verticalOffset,
                () -> {
                    if (toIndex != Position.FRONT) {
                        setOnTouchRunnable(null);
                        setOnTitleChanged(null);
                        mTimer.cancelTimer();
                        // Make it unable to be focused if it is not in the front.
                        mView.enableA11y(false);
                        updateAccessibilityPane(toIndex);
                    } else {
                        mView.enableA11y(true);
                        setOnTouchRunnable(mTimer::resetTimer);
                        updateAccessibilityPane(toIndex);
                        setOnTitleChanged(
                                () -> {
                                    mTimer.resetTimer();
                                    updateAccessibilityPane(toIndex);
                                });
                        mTimer.startTimer(mAutodismissDurationMs.get(), mOnTimeUp);
                    }
                });
    }

    /**
     * Hides the message banner.
     *
     * @param fromIndex The initial position.
     * @param toIndex The target position the message is moving to.
     * @param animate Whether to hide with an animation.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     * @return The animator which hides the message view.
     */
    @Nullable Animator hide(
            @Position int fromIndex,
            @Position int toIndex,
            boolean animate,
            Runnable messageHidden) {
        mView.dismissSecondaryMenuIfShown();
        mTimer.cancelTimer();
        // Skip animation if animation has been globally disabled.
        // Otherwise, child animator's listener's onEnd will be called immediately after onStart,
        // even before parent animatorSet's listener's onStart.
        var isAnimationDisabled = AccessibilityState.prefersReducedMotion();
        return mMediator.hide(
                fromIndex,
                toIndex,
                animate && !isAnimationDisabled,
                () -> {
                    setOnTouchRunnable(null);
                    setOnTitleChanged(null);
                    sendPaneChangeAccessibilityEvent(/* isShowing= */ false);
                    messageHidden.run();
                });
    }

    void cancelTimer() {
        mTimer.cancelTimer();
    }

    void startTimer() {
        mTimer.startTimer(mAutodismissDurationMs.get(), mOnTimeUp);
    }

    void setOnTouchRunnable(@Nullable Runnable runnable) {
        mMediator.setOnTouchRunnable(runnable);
    }

    private void updateAccessibilityPane(int toIndex) {
        String msg = "";
        if (toIndex == Position.FRONT) {
            msg =
                    mModel.get(MessageBannerProperties.TITLE)
                            + " "
                            + mView.getResources().getString(R.string.message_screen_position);
        } else {
            msg = mView.getResources().getString(R.string.message_new_actions_available);
        }
        ViewCompat.setAccessibilityPaneTitle(mParentView, msg);
        sendPaneChangeAccessibilityEvent(/* isShowing= */ true);
    }

    /**
     * Sends accessibility events for pane appearance/disappearance when the message is shown/hidden
     * respectively. This should ideally move accessibility focus automatically to/out of the
     * message view as applicable.
     *
     * @param isShowing Whether the message is visible. {@code true} if shown, {@code false} if
     *     hidden.
     */
    @SuppressWarnings("WrongConstant")
    private void sendPaneChangeAccessibilityEvent(boolean isShowing) {
        // We will only send an AccessibilityEvent when running the
        // MessagesAccessibilityEventInvestigations experiment since this is known to crash in some
        // cases. The experiment will provide more hints to crash root cause and possible solutions.
        if (!MessageFeatureList.isMessagesAccessibilityEventInvestigationsEnabled()) {
            return;
        }

        AccessibilityEvent event =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        if (isShowing) {
            event.setContentChangeTypes(AccessibilityEventCompat.CONTENT_CHANGE_TYPE_PANE_APPEARED);
        } else {
            event.setContentChangeTypes(
                    AccessibilityEventCompat.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED);
        }

        // The MessagesAccessibilityEventInvestigations has various arms to try potential solutions.
        int approach = MessageFeatureList.getMessagesAccessibilityEventInvestigationsParam();
        switch (approach) {
            case AccessibilityEventInvestigationGroup.ENABLED_BASELINE:
                // Case 1 = "EnabledBaseline" group. This is our baseline group, which acts similar
                // to a control group for the experiment. This is the ideal implementation that we
                // would expect not to crash, and we want to compare the other implementations
                // against this one.
                if (AccessibilityState.isAnyAccessibilityServiceEnabled()) {
                    mView.requestSendAccessibilityEvent(mView, event);
                }
                break;
            case AccessibilityEventInvestigationGroup.ENABLED_WITH_ACCESSIBILITY_STATE:
                // Case 2 = "EnabledWithAccessibilityState" group. For this group, we will send the
                // event through the AccessibilityState instead.
                AccessibilityState.sendAccessibilityEvent(event);
                break;
            case AccessibilityEventInvestigationGroup.ENABLED_WITH_RESTRICTIVE_SERVICE_CHECK:
                // Case 3 = "EnabledWithRestrictiveServiceCheck" group. For this group, we will send
                // the event as we normally would, but for a more restrictive group of services.
                if (AccessibilityState.isComplexUserInteractionServiceEnabled()) {
                    mView.requestSendAccessibilityEvent(mView, event);
                }
                break;
            case AccessibilityEventInvestigationGroup.ENABLED_WITH_MASK_CHECK:
                // Case 4 = "EnabledWithMaskCheck" group. For this group, we will not check the
                // enabled services, but instead look at the requested event types, and only send
                // the event if some service has requested its type.
                if (AccessibilityState.relevantEventTypesForCurrentServices()
                        .contains(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED)) {
                    mView.requestSendAccessibilityEvent(mView, event);
                }
                break;
            case AccessibilityEventInvestigationGroup.ENABLED_WITH_DIRECT_QUERY:
                // Case 5 = "EnabledWithDirectQuery" group. For this group, we will directly check
                // the current accessibility state against the framework before sending the event.
                AccessibilityManager manager =
                        (AccessibilityManager)
                                mView.getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
                if (manager.isEnabled()) {
                    mView.requestSendAccessibilityEvent(mView, event);
                }
                break;
            default:
                // For any other value (default or bad param), we do not want to send any event.
                break;
        }
    }

    private void setOnTitleChanged(@Nullable Runnable runnable) {
        mView.setOnTitleChanged(runnable);
    }
}
