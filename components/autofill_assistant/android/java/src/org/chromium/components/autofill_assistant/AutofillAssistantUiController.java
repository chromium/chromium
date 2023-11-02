// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.components.autofill_assistant.carousel.AssistantChip;
import org.chromium.components.autofill_assistant.metrics.DropOutReason;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.autofill_assistant.user_data.GmsIntegrator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 * This controller is purely a translation and forwarding layer between Native side and the
 * different Java coordinators.
 */
@JNINamespace("autofill_assistant")
// TODO(crbug.com/806868): This class should be removed once all logic is in native side and the
// model is directly modified by the native AssistantMediator.
public class AutofillAssistantUiController {
    private static final String TAG = "AutofillAssistant";

    private long mNativeUiController;

    private final Activity mActivity;
    private final AssistantCoordinator mCoordinator;
    private final AssistantDependencies mDependencies;
    private final Destroyable mTabChangeObserverDestroyer;
    private final WebContents mWebContents;

    private final AssistantSnackbarFactory mSnackbarFactory;
    private final AssistantFeedbackUtil mFeedbackUtil;
    @Nullable
    private AssistantSnackbar mSnackbar;

    /**
     * Returns {@code true} if the all dependencies are ready.
     */
    @CalledByNative
    private static boolean canAttachUi(
            WebContents webContents, AssistantDependencies dependencies) {
        return dependencies.maybeUpdateDependencies(webContents);
    }

    @CalledByNative
    private AutofillAssistantUiController(long nativeUiController, WebContents webContents,
            AssistantDependencies dependencies, boolean allowTabSwitching,
            @Nullable AssistantOverlayCoordinator overlayCoordinator) {
        Log.e(TAG, "AutofillAssistantUiController::AutofillAssistantUiController");
        mWebContents = webContents;
        mDependencies = dependencies;
        mActivity = dependencies.getActivity();

        mNativeUiController = nativeUiController;
        mSnackbarFactory = dependencies.getSnackbarFactory();
        mFeedbackUtil = dependencies.createFeedbackUtil();

        // NOTE: Only create one instance of this unless you know what you are doing.
        @Nullable
        AssistantTabObscuringUtil tabObscuringUtil =
                dependencies.getTabObscuringUtilOrNull(dependencies.getWindowAndroid());

        mCoordinator = new AssistantCoordinator(mActivity, dependencies.getBottomSheetController(),
                tabObscuringUtil, overlayCoordinator, this::safeNativeOnKeyboardVisibilityChanged,
                dependencies.getKeyboardVisibilityDelegate(), dependencies.getRootView(),
                dependencies.getRootViewGroup(), dependencies.createBrowserControlsFactory(),
                dependencies.getBottomInsetProvider(), dependencies.getAccessibilityUtil(),
                dependencies.createInfoPageUtil(),
                dependencies.createProfileImageUtilOrNull(
                        mActivity, R.dimen.autofill_assistant_profile_size),
                dependencies.createImageFetcher(), dependencies.createEditorFactory(),
                dependencies.getWindowAndroid(), dependencies.createSettingsUtil());

        mTabChangeObserverDestroyer =
                dependencies.observeTabChanges(new AssistantTabChangeObserver() {
                    @Override
                    public void onObservingDifferentTab(
                            boolean isTabNull, @Nullable WebContents webContents, boolean isHint) {
                        Log.v(TAG,
                                "UiController - onObservingDifferentTab: isTabNull=" + isTabNull
                                        + ", webContentsIsNull=" + (webContents == null)
                                        + ", isHint=" + isHint + ", webContents==mWebContents="
                                        + (webContents == mWebContents));

                        if (mWebContents == null) {
                            if (!isHint) {
                                // This particular scenario would happen only if we're switching
                                // from a tab with no Autofill Assistant running to a tab with AA
                                // running with no tab switching hinting (i.e. a first notification
                                // with |isHint| set to true).
                                // In this case the native side is not yet fully initialized, so we
                                // need to wait for the web contents to be set from native before
                                // notifying native that the tab was selected.
                                setWebContentObserver(isTabNull, webContents);
                            }
                            return;
                        }

                        if (!allowTabSwitching) {
                            if (isTabNull || webContents != mWebContents) {
                                safeNativeOnFatalError(
                                        mActivity.getString(R.string.autofill_assistant_give_up),
                                        DropOutReason.TAB_CHANGED);
                            }
                            return;
                        }

                        // Get rid of any undo snackbars right away before switching tabs, to avoid
                        // confusion.
                        dismissSnackbar();

                        if (isTabNull) {
                            safeOnTabSwitched(getModel().getBottomSheetState(),
                                    /* activityChanged = */ false);
                            // A null tab indicates that there's no selected tab; Most likely, we're
                            // in the process of selecting a new tab. Hide the UI for possible reuse
                            // later.
                            safeNativeSetVisible(false);
                        } else if (webContents == mWebContents) {
                            // The original tab was re-selected. Show it again and force an
                            // expansion on the bottom sheet.
                            if (!isHint) {
                                // Here and below, we're only interested in restoring the UI for the
                                // case where isHint is false, meaning that the tab is shown. This
                                // is the only way to be sure that the bottomsheet is unsuppressed
                                // when we try to restore the status to what it was prior to
                                // switching.
                                safeOnTabSelected();
                            }
                        } else {
                            //
                            safeOnTabSwitched(getModel().getBottomSheetState(),
                                    /* activityChanged = */ false);
                            // A new tab was selected, destroy the UI.
                            @Nullable
                            AutofillAssistantClient client =
                                    AutofillAssistantClient.fromWebContents(mWebContents);
                            if (client != null) {
                                client.destroyUi();
                            }

                            if (!isHint) {
                                safeOnTabSelected();
                            }
                        }
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            @Nullable WebContents webContents, @Nullable WindowAndroid window) {
                        Log.v(TAG,
                                "UiController - onActivityAttachmentChanged: webContentsIsNull="
                                        + (webContents == null)
                                        + ", windowAndroidIsNull=" + (window == null));

                        if (mWebContents == null) return;

                        if (window == null && webContents == mWebContents) {
                            if (!allowTabSwitching) {
                                safeNativeStop(DropOutReason.TAB_DETACHED);
                                return;
                            }

                            safeOnTabSwitched(
                                    getModel().getBottomSheetState(), /* activityChanged = */ true);
                            // If we have an open snackbar, execute the callback immediately. This
                            // may shut down the Autofill Assistant.
                            if (mSnackbar != null) {
                                safeSnackbarResult(false);
                            }

                            @Nullable
                            AutofillAssistantClient client =
                                    AutofillAssistantClient.fromWebContents(mWebContents);
                            if (client != null) {
                                client.destroyUi();
                            }
                        }
                    }
                });
    }

    private void setWebContentObserver(boolean isTabNull, @Nullable WebContents webContents) {
        getModel().addObserver(new PropertyObserver<PropertyKey>() {
            @Override
            public void onPropertyChanged(
                    PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
                if (AssistantModel.WEB_CONTENTS == propertyKey) {
                    getModel().removeObserver(this);
                    if (!isTabNull && webContents == getModel().get(AssistantModel.WEB_CONTENTS)) {
                        safeOnTabSelected();
                    }
                }
            }
        });
    }

    // Native => Java methods.

    // TODO(crbug.com/806868): Some of these functions still have a little bit of logic (e.g. make
    // the progress bar pulse when hiding overlay). Maybe it would be better to forward all calls to
    // AssistantCoordinator (that way this bridge would only have a reference to that one) which in
    // turn will forward calls to the other sub coordinators. The main reason this is not done yet
    // is to avoid boilerplate.

    @CalledByNative
    private AssistantModel getModel() {
        return mCoordinator.getModel();
    }

    @CalledByNative
    private AssistantDependencies getDependencies() {
        return mDependencies;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeUiController = 0;
        mTabChangeObserverDestroyer.destroy();
        mCoordinator.destroy();
    }

    /**
     * Close CCT after the current task has finished running - usually after Autofill Assistant has
     * finished shutting itself down.
     */
    @CalledByNative
    private void scheduleCloseCustomTab() {
        mDependencies.createTabUtil().scheduleCloseCustomTab(mActivity);
    }

    @CalledByNative
    private void showContentAndExpandBottomSheet() {
        mCoordinator.getBottomBarCoordinator().showContent(
                /* shouldExpand = */ true, /* animate = */ true);
    }

    @CalledByNative
    private void expandBottomSheet() {
        mCoordinator.getBottomBarCoordinator().expand();
    }

    @CalledByNative
    private void collapseBottomSheet() {
        mCoordinator.getBottomBarCoordinator().collapse();
    }

    /**
     * Shows a feedback form.
     */
    @CalledByNative
    private void showFeedback(String debugContext, /* @ScreenshotMode */ int screenshotMode) {
        mFeedbackUtil.showFeedback(mActivity, mWebContents, screenshotMode, debugContext);
    }

    @CalledByNative
    private boolean isKeyboardShown() {
        return mCoordinator.getKeyboardCoordinator().isKeyboardShown();
    }

    @CalledByNative
    private void hideKeyboard() {
        mCoordinator.getKeyboardCoordinator().hideKeyboard();
    }

    @CalledByNative
    private void restoreBottomSheetState(@SheetState int state) {
        mCoordinator.getBottomBarCoordinator().restoreState(state);
    }

    @CalledByNative
    private void hideKeyboardIfFocusNotOnText() {
        mCoordinator.getKeyboardCoordinator().hideKeyboardIfFocusNotOnText();
    }

    @CalledByNative
    private void showSnackbar(int delayMs, String message, String undoString) {
        mSnackbar = mSnackbarFactory.createSnackbar(
                delayMs, message, undoString, this::safeSnackbarResult);
        mSnackbar.show();
    }

    @CalledByNative
    private void showGmsAccountScreenIntent(int screenId, String emailAddress) {
        WebContents webContents = getModel().get(AssistantModel.WEB_CONTENTS);
        new GmsIntegrator(emailAddress, mActivity)
                .launchAccountIntent(screenId, webContents.getTopLevelNativeWindow(), unused -> {});
    }

    private void dismissSnackbar() {
        if (mSnackbar != null) {
            mSnackbar.dismiss();
            mSnackbar = null;
        }
    }

    /** Creates an empty list of chips. */
    @CalledByNative
    private static List<AssistantChip> createChipList() {
        return new ArrayList<>();
    }

    /**
     * Creates an action button which executes the action {@code actionIndex}.
     */
    @CalledByNative
    private AssistantChip createActionButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription, "action_" + actionIndex);
        chip.setSelectedListener(() -> safeNativeOnUserActionSelected(actionIndex));
        return chip;
    }

    /**
     * Creates a highlighted action button which executes the action {@code actionIndex}.
     */
    @CalledByNative
    private AssistantChip createHighlightedActionButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHighlightedAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription, "action_" + actionIndex);
        chip.setSelectedListener(() -> safeNativeOnUserActionSelected(actionIndex));
        return chip;
    }

    /**
     * Creates a cancel action button. If the keyboard is currently shown, it dismisses the
     * keyboard. Otherwise, it shows the snackbar and then executes {@code actionIndex}, or shuts
     * down Autofill Assistant if {@code actionIndex} is {@code -1}.
     */
    @CalledByNative
    private AssistantChip createCancelButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription, "cancel_" + actionIndex);
        chip.setSelectedListener(() -> safeNativeOnCancelButtonClicked(actionIndex));
        return chip;
    }

    /**
     * Adds a close action button to the chip list, which shuts down Autofill Assistant.
     */
    @CalledByNative
    private AssistantChip createCloseButton(int icon, String text, boolean disabled, boolean sticky,
            boolean visible, @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription, "close");

        chip.setSelectedListener(() -> safeNativeOnCloseButtonClicked());
        return chip;
    }

    /**
     * Creates a feedback button button. It shows the feedback form and then *directly* executes
     * {@code actionIndex}.
     */
    @CalledByNative
    private AssistantChip createFeedbackButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(icon, text, disabled, sticky,
                visible, contentDescription, "feedback_" + actionIndex);
        chip.setSelectedListener(() -> safeNativeOnFeedbackButtonClicked(actionIndex));
        return chip;
    }

    // TODO(arbesser): Remove this and use methods in {@code AssistantChip} instead.
    @CalledByNative
    private static void appendChipToList(List<AssistantChip> chips, AssistantChip chip) {
        chips.add(chip);
    }

    @CalledByNative
    private void setActions(List<AssistantChip> chips) {
        // TODO(b/144075373): Move this to AssistantCarouselModel.
        getModel().getActionsModel().setChips(chips);
    }

    @CalledByNative
    private void setDisableChipChangeAnimations(boolean disable) {
        // TODO(b/144075373): Move this to AssistantCarouselModel.
        getModel().getActionsModel().setDisableChangeAnimations(disable);
    }

    @CalledByNative
    private void setViewportMode(@AssistantViewportMode int mode) {
        mCoordinator.getBottomBarCoordinator().setViewportMode(mode);
    }

    @CalledByNative
    private void setPeekMode(@AssistantPeekHeightCoordinator.PeekMode int peekMode) {
        mCoordinator.getBottomBarCoordinator().setPeekMode(peekMode);
    }

    @CalledByNative
    private Context getContext() {
        return mActivity;
    }

    @CalledByNative
    private int[] getWindowSize() {
        Window window = mDependencies.getActivity().getWindow();
        if (window == null) {
            return null;
        }
        return new int[] {window.getDecorView().getWidth(), window.getDecorView().getHeight()};
    }

    @CalledByNative
    private int getScreenOrientation() {
        return mDependencies.getActivity().getResources().getConfiguration().orientation;
    }

    // Native methods.
    private void safeSnackbarResult(boolean undo) {
        if (mSnackbar != null && mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().snackbarResult(
                    mNativeUiController, AutofillAssistantUiController.this, undo);
        }
        mSnackbar = null;
    }

    private void safeNativeStop(@DropOutReason int reason) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().stop(
                    mNativeUiController, AutofillAssistantUiController.this, reason);
        }
    }

    private void safeNativeOnFatalError(String message, @DropOutReason int reason) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onFatalError(
                    mNativeUiController, AutofillAssistantUiController.this, message, reason);
        }
    }

    private void safeNativeOnUserActionSelected(int index) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onUserActionSelected(
                    mNativeUiController, AutofillAssistantUiController.this, index);
        }
    }

    private void safeNativeOnCancelButtonClicked(int index) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onCancelButtonClicked(
                    mNativeUiController, AutofillAssistantUiController.this, index);
        }
    }

    private void safeNativeOnCloseButtonClicked() {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onCloseButtonClicked(
                    mNativeUiController, AutofillAssistantUiController.this);
        }
    }

    private void safeNativeOnFeedbackButtonClicked(int index) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onFeedbackButtonClicked(
                    mNativeUiController, AutofillAssistantUiController.this, index);
        }
    }

    private void safeNativeOnKeyboardVisibilityChanged(boolean visible) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onKeyboardVisibilityChanged(
                    mNativeUiController, AutofillAssistantUiController.this, visible);
        }
    }

    private void safeNativeSetVisible(boolean visible) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().setVisible(
                    mNativeUiController, AutofillAssistantUiController.this, visible);
        }
    }

    private void safeOnTabSwitched(@SheetState int state, boolean activityChanged) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onTabSwitched(mNativeUiController,
                    AutofillAssistantUiController.this, state, activityChanged);
        }
    }

    private void safeOnTabSelected() {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onTabSelected(
                    mNativeUiController, AutofillAssistantUiController.this);
        }
    }

    @NativeMethods
    interface Natives {
        void snackbarResult(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, boolean undo);
        void stop(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                @DropOutReason int reason);
        void onFatalError(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                String message, @DropOutReason int reason);
        void onUserActionSelected(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, int index);
        void onCancelButtonClicked(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, int index);
        void onCloseButtonClicked(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller);
        void onFeedbackButtonClicked(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, int index);
        void onKeyboardVisibilityChanged(long nativeUiControllerAndroid,
                AutofillAssistantUiController caller, boolean visible);
        void setVisible(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                boolean visible);
        void onTabSwitched(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                @SheetState int state, boolean activityChanged);
        void onTabSelected(long nativeUiControllerAndroid, AutofillAssistantUiController caller);
    }
}
