// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * <p>
 * Public implementation that presents survey with a Clank message. Once a survey is ready to show,
 * the delegate will wait until the current tab is fully loaded and display a survey message. The
 * client features using this implementation will need to supply a {@link PropertyModel} with
 * {@link MessageBannerProperties}.
 * </p>
 *
 * <p>
 * Several things to be aware of:
 * <ul>
 *   <li>
 *     Survey request will be dropped if user is in or switched into incognito mode.
 *   </li>
 *   <li>
 *     {@link MessageBannerProperties#ON_PRIMARY_ACTION} and {@link
 *      MessageBannerProperties#ON_DISMISSED} will be wrapped in a new callback within this class in
 *      order to trigger / clean up surveys accordingly.
 *   </li>
 *   <li>
 *     Due to the nature of message system, the survey invitation is not always guaranteed to be
 *     shown. If {@link MessageBannerProperties#ON_PRIMARY_ACTION} is provided in the input {@link
 *     PropertyModel}, it'll be called once survey invitation is accepted.
 *  </li>
 * </ul>
 *</p>
 */
public class MessageSurveyUiDelegate implements SurveyUiDelegate {
    /**
     * Internal state about the survey message state. Mostly used for debugging / troubleshooting.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        State.NOT_STARTED,
        State.REQUESTED,
        State.ENQUEUED,
        State.ACCEPTED,
        State.DISMISSED,
        State.NOT_PRESENTED
    })
    @interface State {
        /** State after Instance created, before {@link #showSurveyInvitation} is called. */
        int NOT_STARTED = 0;

        /** State after {@link #showSurveyInvitation} is called, before message is enqueued. */
        int REQUESTED = 1;

        /**
         * State after  {@link MessageDispatcher#enqueueWindowScopedMessage}, before message is
         * dismissed / accepted.
         */
        int ENQUEUED = 2;

        /**
         * State after message is presented and dismissed by  {@link MessageDispatcher}. The
         * instance will no longer switch into any other state.
         */
        int DISMISSED = 3;

        /**
         * State after message is presented and accepted by the user. The instance will no longer
         * switch into any other state.
         */
        int ACCEPTED = 4;

        /**
         * State after message is failed to present. This happened at any point and before survey is
         * presented. Once survey is presented, it should end up as either {@link #DISMISSED} or
         * {@link #ACCEPTED}.
         */
        int NOT_PRESENTED = 5;
    }

    private final PropertyModel mMessageModel;
    private final MessageDispatcher mMessageDispatcher;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<Boolean> mCrashUploadPermissionSupplier;

    private @State int mState;

    /**
     * Runnables used to notify SurveyClient for the invitation outcome. Null before
     * #showSurveyInvitation is called, or the delegate class is teared down.
     */
    private @Nullable Runnable mOnSurveyAccepted;

    private @Nullable Runnable mOnSurveyDeclined;
    private @Nullable Runnable mOnSurveyPresentationFailed;

    private @Nullable Tab mSurveyPromptTab;
    private @Nullable Tab mLoadingTab;
    private @Nullable TabModelSelectorObserver mTabModelSelectorObserver;
    private @Nullable TabObserver mDismissMessageTabObserver;
    private @Nullable TabObserver mLoadingTabObserver;

    /**
     * Create a survey UI delegate that presents the survey with a message on the current tab.
     *
     * @param customModel Custom model to provide survey message Id, title, icon, etc. Note that the
     *         primary and dismissal action will be wrapped around survey callbacks in order for
     *         accepting survey to work properly.
     * @param messageDispatcher Dispatcher used to enqueue survey messages. Usually from {@link
     *         MessageDispatcherProvider#from(WindowAndroid)}
     * @param modelSelector TabModel selector used to retrieve tab information. Used to decide if
     *         current tab is fully loaded and not in incognito.
     */
    public MessageSurveyUiDelegate(
            @NonNull PropertyModel customModel,
            @NonNull MessageDispatcher messageDispatcher,
            @NonNull TabModelSelector modelSelector,
            Supplier<Boolean> crashUploadPermissionSupplier) {
        mMessageModel = customModel;
        mTabModelSelector = modelSelector;
        mMessageDispatcher = messageDispatcher;
        mCrashUploadPermissionSupplier = crashUploadPermissionSupplier;

        mState = State.NOT_STARTED;
    }

    /**
     * Create a message model with default title, icon, and button text used for survey, if they are
     * not provided in the input model.
     *
     * @param resources {@link Resources} used to retrieve string and icon.
     * @param model The input model.
     * @return The model with title / icon / primary button text to be used.
     */
    public static PropertyModel populateDefaultValuesForSurveyMessage(
            Resources resources, @NonNull PropertyModel model) {
        if (model.get(MessageBannerProperties.ICON_RESOURCE_ID) == 0) {
            model.set(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.fre_product_logo);
            model.set(MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE);
        }
        if (TextUtils.isEmpty(model.get(MessageBannerProperties.TITLE))) {
            model.set(
                    MessageBannerProperties.TITLE,
                    resources.getString(R.string.chrome_survey_message_title));
        }
        if (TextUtils.isEmpty(model.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT))) {
            model.set(
                    MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                    resources.getString(R.string.chrome_survey_message_button));
        }
        return model;
    }

    @Override
    public void showSurveyInvitation(
            Runnable onSurveyAccepted,
            Runnable onSurveyDeclined,
            Runnable onSurveyPresentationFailed) {
        assert mState == State.NOT_STARTED : "showSurveyInvitation should only be called once.";

        mState = State.REQUESTED;
        mOnSurveyAccepted = onSurveyAccepted;
        mOnSurveyDeclined = onSurveyDeclined;
        mOnSurveyPresentationFailed = onSurveyPresentationFailed;

        showSurveyIfReady();
    }

    @Override
    public void dismiss() {
        // If message is already enqueued, delegate the message dispatcher to trigger dismiss.
        if (mState < State.ENQUEUED) {
            cancel();
        } else if (mState == State.ENQUEUED) {
            dismissMessage(DismissReason.DISMISSED_BY_FEATURE);
        }
    }

    private void dismissMessage(@DismissReason int dismissReason) {
        // Let the message dispatcher dismiss the message, which will eventually call #onDismiss for
        // the message if the message is in the queue.
        mMessageDispatcher.dismissMessage(mMessageModel, dismissReason);
        destroy();
    }

    private void cancel() {
        if (mState <= State.ENQUEUED) {
            mState = State.NOT_PRESENTED;
            runIfNotNull(mOnSurveyPresentationFailed);
        }
        destroy();
    }

    /**
     * @return Whether survey can be shown in the current session.
     */
    private boolean canShowSurveyPrompt() {
        return !mTabModelSelector.isIncognitoSelected()
                && Boolean.TRUE.equals(mCrashUploadPermissionSupplier.get());
    }

    private void showSurveyIfReady() {
        assert mTabModelSelectorObserver == null;
        assert mLoadingTabObserver == null;
        assert mState < State.ENQUEUED;

        if (!canShowSurveyPrompt()) {
            cancel();
            return;
        }

        // Wait until tab model has an active tab.
        if (mTabModelSelector.getCurrentTab() == null) {
            removeLoadingTabReferences();
            mTabModelSelectorObserver =
                    new TabModelSelectorObserver() {
                        @Override
                        public void onChange() {
                            mTabModelSelector.removeObserver(this);
                            mTabModelSelectorObserver = null;
                            showSurveyIfReady();
                        }
                    };
            mTabModelSelector.addObserver(mTabModelSelectorObserver);
            return;
        }

        // Wait until tab is ready.
        Tab tab = mTabModelSelector.getCurrentTab();
        if (waitUntilTabReadyForSurvey(tab)) {
            showSurveyMessage(mTabModelSelector.getCurrentTab());
        }
    }

    private void showSurveyMessage(Tab tab) {
        assert tab != null;

        mSurveyPromptTab = tab;

        // Wrap calls with callbacks from #showSurveyInvitations.
        Supplier<Integer> wrappedOnAcceptAction =
                mMessageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION);
        mMessageModel.set(
                MessageBannerProperties.ON_PRIMARY_ACTION,
                () -> {
                    mState = State.ACCEPTED;
                    runIfNotNull(mOnSurveyAccepted);
                    if (wrappedOnAcceptAction != null) {
                        wrappedOnAcceptAction.get();
                    }
                    destroy();
                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                });
        Callback<Integer> wrappedOnDismissCallback =
                mMessageModel.get(MessageBannerProperties.ON_DISMISSED);
        mMessageModel.set(
                MessageBannerProperties.ON_DISMISSED,
                (reason) -> {
                    if (reason != DismissReason.PRIMARY_ACTION) {
                        runIfNotNull(mOnSurveyDeclined);
                        mState = State.DISMISSED;
                    }
                    if (wrappedOnDismissCallback != null) {
                        wrappedOnDismissCallback.onResult(reason);
                    }
                    destroy();
                });

        // Dismiss the message when the original tab in which the message is shown is
        // hidden. This prevents the prompt from being shown if the tab is opened after being
        // hidden for a duration in which the survey expired. See crbug.com/1249055 for details.
        mDismissMessageTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        tab.removeObserver(this);
                        dismissMessage(DismissReason.TAB_SWITCHED);
                    }
                };
        mSurveyPromptTab.addObserver(mDismissMessageTabObserver);

        mMessageDispatcher.enqueueWindowScopedMessage(mMessageModel, false);
        mState = State.ENQUEUED;
    }

    private boolean waitUntilTabReadyForSurvey(@NonNull Tab loadingTab) {
        assert mLoadingTab == null;
        mLoadingTab = loadingTab;

        if (isTabReadyForSurvey(mLoadingTab)) {
            return true;
        }

        mLoadingTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        if (!isTabReadyForSurvey(mLoadingTab) || !isInteractable) return;
                        removeLoadingTabReferences();
                        showSurveyIfReady();
                    }

                    @Override
                    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                        if (!isTabReadyForSurvey(mLoadingTab)) return;
                        removeLoadingTabReferences();
                        showSurveyIfReady();
                    }

                    @Override
                    public void onHidden(Tab tab, /*@TabHidingType*/ int type) {
                        // A prompt shouldn't appear on a tab that the user has left.
                        removeLoadingTabReferences();
                        cancel();
                    }
                };

        mLoadingTab.addObserver(mLoadingTabObserver);
        return false;
    }

    private static boolean isTabReadyForSurvey(Tab tab) {
        return !tab.isLoading() && tab.isUserInteractable();
    }

    private void removeLoadingTabReferences() {
        if (mLoadingTab == null) return;

        mLoadingTab.removeObserver(mLoadingTabObserver);
        mLoadingTab = null;
        mLoadingTabObserver = null;
    }

    private void destroy() {
        if (mSurveyPromptTab != null && mDismissMessageTabObserver != null) {
            mSurveyPromptTab.removeObserver(mDismissMessageTabObserver);
        }
        if (mLoadingTab != null && mLoadingTabObserver != null) {
            mLoadingTab.removeObserver(mLoadingTabObserver);
        }
        if (mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        mTabModelSelectorObserver = null;
        mLoadingTab = null;
        mLoadingTabObserver = null;
        mSurveyPromptTab = null;
        mDismissMessageTabObserver = null;
        mOnSurveyAccepted = null;
        mOnSurveyDeclined = null;
        mOnSurveyPresentationFailed = null;
    }

    private void runIfNotNull(Runnable runnable) {
        if (runnable != null) runnable.run();
    }

    @State
    int getStateForTesting() {
        return mState;
    }
}
