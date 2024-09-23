// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.activity.ComponentDialog;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A base class for presenting a single tab modal dialog.
 *
 * Several abstract methods allow embedder-specific specializations.
 */
public abstract class TabModalPresenter extends ModalDialogManager.Presenter {
    /** Enter and exit animation duration. */
    private static final int ENTER_EXIT_ANIMATION_DURATION_MS = 200;

    private final Context mContext;

    private ViewGroup mDialogContainer;

    private ModalDialogView mDialogView;

    private PropertyModelChangeProcessor<PropertyModel, ModalDialogView, PropertyKey>
            mModelChangeProcessor;

    /** Whether the action bar on selected text is temporarily cleared for showing dialogs. */
    private boolean mDidClearTextControls;

    /**
     * Whether the dialog should gain focus for accessibility when in front, determined by the
     * dialog {@link ModalDialogProperties} FOCUS_DIALOG property.
     */
    private boolean mFocusDialog;

    private class ViewBinder extends ModalDialogViewBinder {
        @Override
        public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
            if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
                assert mDialogContainer != null;
                if (model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE)) {
                    mDialogContainer.setOnClickListener(
                            (v) -> {
                                dismissCurrentDialog(DialogDismissalCause.TOUCH_OUTSIDE);
                            });
                } else {
                    mDialogContainer.setOnClickListener(null);
                }
            } else if (ModalDialogProperties.FOCUS_DIALOG == propertyKey) {
                if (model.get(ModalDialogProperties.FOCUS_DIALOG)) {
                    mFocusDialog = true;
                }
            } else if (ModalDialogProperties.TAB_MODAL_DIALOG_CANCEL_ON_ESCAPE == propertyKey) {
                if (model.get(ModalDialogProperties.TAB_MODAL_DIALOG_CANCEL_ON_ESCAPE)) {
                    view.setOnEscapeCallback(
                            () -> {
                                dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK);
                            });
                } else {
                    view.setOnEscapeCallback(null);
                }
            } else {
                super.bind(model, view, propertyKey);
            }
        }
    }

    /**
     * Constructor for initializing dialog container.
     * @param context The context for inflating UI.
     */
    public TabModalPresenter(Context context) {
        mContext = context;
    }

    /** @return a ViewGroup that will host {@link TabModalPresenter#mDialogView}. */
    protected abstract ViewGroup createDialogContainer();

    /** Called when {@link TabModalPresenter#mDialogContainer} should be displayed. */
    protected abstract void showDialogContainer();

    /**
     * Set whether the browser controls access should be restricted.
     *
     * This is called any time a dialog view is being shown or hidden and should update browser
     * state, e.g. breaking fullscreen or disabling certain browser controls as necessary.
     *
     * @param restricted Whether the browser controls access should be restricted.
     */
    protected abstract void setBrowserControlsAccess(boolean restricted);

    /** @return the container previously returned by {@link TabModalPresenter#createDialogContainer}. */
    protected ViewGroup getDialogContainer() {
        return mDialogContainer;
    }

    private ModalDialogView loadDialogView(int style) {
        return (ModalDialogView)
                LayoutInflaterUtils.inflate(
                        new ContextThemeWrapper(mContext, style), R.layout.modal_dialog_view, null);
    }

    @Override
    protected void addDialogView(
            PropertyModel model, @Nullable Callback<ComponentDialog> onDialogCreatedCallback) {
        if (mDialogContainer == null) mDialogContainer = createDialogContainer();

        model.set(ModalDialogProperties.TAB_MODAL_DIALOG_CANCEL_ON_ESCAPE, true);
        int style = R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton;
        int buttonStyles = model.get(ModalDialogProperties.BUTTON_STYLES);
        if (buttonStyles == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE
                || buttonStyles == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE) {
            style = R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton;
        } else if (buttonStyles
                == ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED) {
            style = R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton;
        }
        mDialogView = loadDialogView(style);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(model, mDialogView, new ViewBinder());
        if (onDialogCreatedCallback != null) {
            onDialogCreatedCallback.onResult(null);
        }

        setBrowserControlsAccess(true);

        showDialogContainer();
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        setBrowserControlsAccess(false);

        // The dialog view may not have been added to the container yet, e.g. if the enter animation
        // has not yet started.
        if (ViewCompat.isAttachedToWindow(mDialogView)) {
            runExitAnimation();
        } else {
            // Cancel any existing animations as when the animation completes it may try to make use
            // of objects that have been set to null.
            mDialogContainer.animate().cancel();
        }

        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
        mDialogView = null;
    }

    /**
     * Change view hierarchy for the dialog container to be either the front most or beneath the
     * toolbar.
     *
     * @param toFront Whether the dialog container should be brought to the front.
     */
    public void updateContainerHierarchy(boolean toFront) {
        if (toFront) {
            mDialogView.announceForAccessibility(getContentDescription(getDialogModel()));
            mDialogView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
            mDialogView.requestFocus();
            if (mFocusDialog) {
                mDialogView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }
        } else {
            mDialogView.clearFocus();
            mDialogView.setImportantForAccessibility(
                    View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        }
    }

    /**
     * Dismisses the text selection action bar that would otherwise obscure a visible dialog, but
     * preserves the text selection.
     *
     * @param webContents the WebContents that the dialog is showing over.
     * @param save true if a dialog is showing and text selection should be saved; false if a dialog
     *         is hiding and text selection should be restored.
     */
    protected void saveOrRestoreTextSelection(@NonNull WebContents webContents, boolean save) {
        if (save) {
            // Dismiss the action bar that obscures the dialogs but preserve the text selection.
            SelectionPopupController controller =
                    SelectionPopupController.fromWebContents(webContents);
            controller.setPreserveSelectionOnNextLossOfFocus(true);
            webContents.getViewAndroidDelegate().getContainerView().clearFocus();
            controller.updateTextSelectionUI(false);
            mDidClearTextControls = true;
        } else if (mDidClearTextControls) {
            // Show the action bar back if it was dismissed when the dialogs were showing.
            mDidClearTextControls = false;
            SelectionPopupController.fromWebContents(webContents).updateTextSelectionUI(true);
        }
    }

    /**
     * Inserts {@link TabModalPresenter#mDialogView} into {@link TabModalPresenter#mDialogContainer}
     * and animates the container into view.
     *
     * Exposed to subclasses as they may want to control the exact start time of the animation.
     */
    protected void runEnterAnimation() {
        mDialogContainer.animate().cancel();

        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER);
        mDialogView.setBackgroundResource(R.drawable.dialog_bg_no_shadow);
        mDialogContainer.addView(mDialogView, params);
        mDialogContainer.setAlpha(0f);
        mDialogContainer.setVisibility(View.VISIBLE);
        mDialogContainer
                .animate()
                .setDuration(ENTER_EXIT_ANIMATION_DURATION_MS)
                .alpha(1f)
                .setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR)
                .setListener(
                        new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationStart(Animator animation) {
                                mDialogView.onEnterAnimationStarted(animation.getDuration());
                            }

                            @Override
                            public void onAnimationEnd(Animator animation) {
                                updateContainerHierarchy(true);
                            }
                        })
                .start();
    }

    private void runExitAnimation() {
        final View dialogView = mDialogView;
        // Clear focus so that keyboard can hide accordingly while entering tab switcher.
        dialogView.clearFocus();
        mDialogContainer.animate().cancel();
        mDialogContainer
                .animate()
                .setDuration(ENTER_EXIT_ANIMATION_DURATION_MS)
                .alpha(0f)
                .setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR)
                .setListener(
                        new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(Animator animation) {
                                mDialogContainer.setVisibility(View.GONE);
                                mDialogContainer.removeView(dialogView);
                            }
                        })
                .start();
    }

    public View getDialogContainerForTest() {
        return mDialogContainer;
    }
}
