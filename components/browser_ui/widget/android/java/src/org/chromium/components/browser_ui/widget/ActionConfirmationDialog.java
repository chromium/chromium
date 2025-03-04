// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.core.util.Function;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;

/** Dialog that asks the user if they're certain they want to perform and action. */
@NullMarked
public class ActionConfirmationDialog {
    /**
     * Returned from {@link ConfirmationDialogHandler#onDialogInteracted} to covey the dismissal
     * behavior.
     */
    @IntDef({DialogDismissType.DISMISS_IMMEDIATELY, DialogDismissType.DISMISS_LATER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogDismissType {
        /** Dismiss the modal dialog immediately. */
        int DISMISS_IMMEDIATELY = 0;

        /** Configure dismissal to happen later via {@link DismissHandler#dismissBlocking(int)}. */
        int DISMISS_LATER = 1;
    }

    @FunctionalInterface
    public interface ConfirmationDialogHandler {
        /**
         * Called when the dialog is dismissed.
         *
         * @param dismissHandler For handling dismissal of the dialog. Callers can use this to
         *     configure async dismissal. Must call {@link #dismissBlocking()} and return {@link
         *     DialogDismissType.DISMISS_LATER} if used.
         * @param buttonClickResult The button click result from the dialog. If the type is {@link
         *     ButtonClickResult.NO_CLICK} then this must return {@link DISMISS_IMMEDIATELY}. Note
         *     this means it is not permitted to use {@code dismissHandler}.
         * @param stopShowing If the user wants to stop showing this dialog in the future. Will be
         *     false if stop showing is not supported.
         * @return whether to dismiss the dialog now, or in future.
         */
        @DialogDismissType
        int onDialogInteracted(
                DismissHandler dismissHandler,
                @ButtonClickResult int buttonClickResult,
                boolean stopShowing);
    }

    /** Handles dismissals for the dialog. */
    public interface DismissHandler {
        /**
         * Blocks dismissal until the returned {@link Runnable} is run. May dismiss on a timeout if
         * this condition is not fulfilled within a minimum time threshold.
         *
         * @param buttonClickResult The button to show the blocking spinner on. It is not valid to
         *     pass {@link ButtonClickResult.NO_CLICK}.
         * @return a {@link Runnable} to execute when the dialog can be dismissed.
         */
        Runnable dismissBlocking(@ButtonClickResult int buttonClickResult);
    }

    @FunctionalInterface
    private interface StopShowingDelegate {
        /** Returns whether to stop showing. */
        boolean shouldStopShowing(@ButtonClickResult int buttonClickResult);
    }

    /**
     * Implementation of the {@link DismissHandler} and {@link ModalDialogProperties.Controller} for
     * a given modal dialog. Implemented as an inner class to ensure that it is scoped to the
     * lifetime of a single modal dialog such that {@link ActionConfirmationDialog} itself is not
     * bound to the lifecycle of any one modal dialog it is facilitating.
     */
    private static class DismissHandlerImpl
            implements DismissHandler, ModalDialogProperties.Controller {
        private static final long SPINNER_MIN_DURATION_MS = 300L;
        private static final long TIMEOUT_DURATION_MS = 10_000L;

        @IntDef({BarrierId.BLOCKING_CONDITION_MET, BarrierId.SPINNER_MIN_DURATION})
        @Retention(RetentionPolicy.SOURCE)
        private @interface BarrierId {
            int BLOCKING_CONDITION_MET = 0;
            int SPINNER_MIN_DURATION = 1;
        }

        private final CallbackController mCallbackController = new CallbackController();
        private final ModalDialogManager mModalDialogManager;
        private final ConfirmationDialogHandler mConfirmationDialogHandler;
        private final StopShowingDelegate mStopShowingDelegate;

        private @Nullable PropertyModel mPropertyModel;
        private @Nullable ArrayList<Integer> mBarrier;
        private @Nullable Runnable mBarrierDismissRunnable;
        private boolean mDismissBlockingCalled;
        private boolean mModalDialogClickHandled;

        DismissHandlerImpl(
                ModalDialogManager modalDialogManager,
                ConfirmationDialogHandler confirmationDialogHandler,
                StopShowingDelegate stopShowingDelegate) {
            mModalDialogManager = modalDialogManager;
            mConfirmationDialogHandler = confirmationDialogHandler;
            mStopShowingDelegate = stopShowingDelegate;
        }

        /**
         * This class' implementation of {@link ModalDialogProperties.Controller} means it needs to
         * exist before the {@link PropertyModel} is built. This method allows deferred init of the
         * {@link PropertyModel} field to circumvent this.
         */
        void setPropertyModel(PropertyModel propertyModel) {
            mPropertyModel = propertyModel;
        }

        @Override
        public void onClick(PropertyModel model, @ButtonType int buttonType) {
            maybeDispatchInteraction(getButtonClickResult(buttonType));
        }

        @Override
        public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
            maybeDispatchInteraction(ButtonClickResult.NO_CLICK);

            // Once dismissed we are done with this object and it is safe to clean up.
            destroy();
        }

        @Override
        public Runnable dismissBlocking(@ButtonClickResult int buttonClickResult) {
            assert mPropertyModel != null;
            assert buttonClickResult != ButtonClickResult.NO_CLICK;

            mDismissBlockingCalled = true;

            // Setup blocking UI. Note that pressing back, closing Chrome, etc. will still exit the
            // scrim.
            mPropertyModel.set(ModalDialogProperties.BLOCK_INPUTS, true);
            mPropertyModel.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false);

            mBarrierDismissRunnable =
                    () -> dismissDialog(getDialogDismissalCause(buttonClickResult));
            mBarrier =
                    new ArrayList<>(
                            Arrays.asList(
                                    BarrierId.BLOCKING_CONDITION_MET,
                                    BarrierId.SPINNER_MIN_DURATION));
            postCancelableOnUiThread(
                    () -> removeFromBarrier(BarrierId.SPINNER_MIN_DURATION),
                    SPINNER_MIN_DURATION_MS);
            postCancelableOnUiThread(
                    () -> dismissDialog(DialogDismissalCause.CLIENT_TIMEOUT), TIMEOUT_DURATION_MS);

            // Timeout to prevent the modal dialog from being onscreen forever.
            return mCallbackController.makeCancelable(
                    () -> removeFromBarrier(BarrierId.BLOCKING_CONDITION_MET));
        }

        private void maybeDispatchInteraction(@ButtonClickResult int buttonClickResult) {
            // Prevent re-entrancy. Clicking the button again or any non-button dismissal will no-op
            // after the first dispatch.
            if (mModalDialogClickHandled) return;

            mModalDialogClickHandled = true;

            @DialogDismissType
            int dismissType =
                    mConfirmationDialogHandler.onDialogInteracted(
                            this,
                            buttonClickResult,
                            mStopShowingDelegate.shouldStopShowing(buttonClickResult));
            if (dismissType == DialogDismissType.DISMISS_IMMEDIATELY) {
                assert !mDismissBlockingCalled
                        : "Cannot invoke dismissBlocking() if using immediate dismissal";

                // If the result is NO_CLICK the dialog is already dismissing automatically so there
                // is no need to signal it to dismiss again.
                if (buttonClickResult != ButtonClickResult.NO_CLICK) {
                    dismissDialog(getDialogDismissalCause(buttonClickResult));
                }
            } else {
                assert mDismissBlockingCalled
                        : "Must invoke dismissBlocking() if using delayed dismissal.";
                assert buttonClickResult != ButtonClickResult.NO_CLICK
                        : "Delayed dismissal not possible without button press.";

                // Dismissal was already configured in dismissBlocking().
            }
        }

        private void destroy() {
            mBarrierDismissRunnable = null;
            mCallbackController.destroy();
        }

        private void postCancelableOnUiThread(Runnable runnable, long delay) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT, mCallbackController.makeCancelable(runnable), delay);
        }

        private void removeFromBarrier(Integer barrierId) {
            assert barrierId != null;
            assert mBarrier != null;

            boolean removed = mBarrier.remove(barrierId);
            assert removed : "Repeate call to removeFromBarrier " + barrierId.intValue();

            if (mBarrier.isEmpty() && mBarrierDismissRunnable != null) {
                mBarrierDismissRunnable.run();
            }
        }

        private void dismissDialog(@DialogDismissalCause int dialogDismissType) {
            mBarrierDismissRunnable = null;

            mModalDialogManager.dismissDialog(mPropertyModel, dialogDismissType);
        }

        private static @ButtonClickResult int getButtonClickResult(@ButtonType int buttonType) {
            return buttonType == ModalDialogProperties.ButtonType.POSITIVE
                    ? ButtonClickResult.POSITIVE
                    : ButtonClickResult.NEGATIVE;
        }

        private static @DialogDismissalCause int getDialogDismissalCause(
                @ButtonClickResult int buttonClickResult) {
            assert buttonClickResult != ButtonClickResult.NO_CLICK;

            return buttonClickResult == ButtonClickResult.POSITIVE
                    ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                    : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
        }
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;

    /**
     * @param context The context to use for resources.
     * @param modalDialogManager The global modal dialog manager.
     */
    public ActionConfirmationDialog(Context context, ModalDialogManager modalDialogManager) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * Shows an action confirmation dialog.
     *
     * @param titleResolver Resolves a title for the dialog.
     * @param descriptionResolver Resolves a description for the dialog.
     * @param positiveButtonRes The string to show for the positive button.
     * @param negativeButtonRes The string to show for the negative button.
     * @param supportStopShowing Whether to show a checkbox to permanently disable the dialog via a
     *     pref.
     * @param confirmationDialogHandler The callback to invoke on exit of the dialog.
     */
    public void show(
            Function<Resources, String> titleResolver,
            Function<Resources, String> descriptionResolver,
            @StringRes int positiveButtonRes,
            @StringRes int negativeButtonRes,
            boolean supportStopShowing,
            ConfirmationDialogHandler confirmationDialogHandler) {
        Resources resources = mContext.getResources();

        View customView =
                LayoutInflater.from(mContext).inflate(R.layout.action_confirmation_dialog, null);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setVisibility(supportStopShowing ? View.VISIBLE : View.GONE);

        String descriptionText = descriptionResolver.apply(resources);
        descriptionTextView.setText(descriptionText);

        StopShowingDelegate stopShowingDelegate =
                (buttonClickResult) -> {
                    // Only remember to stop showing when a button is clicked. Otherwise the user
                    // may have just wanted to cancel.
                    return buttonClickResult != ButtonClickResult.NO_CLICK
                            && stopShowingCheckBox.isChecked();
                };

        String titleText = titleResolver.apply(resources);
        String positiveText = resources.getString(positiveButtonRes);
        String negativeText = resources.getString(negativeButtonRes);

        DismissHandlerImpl dismissHandler =
                new DismissHandlerImpl(
                        mModalDialogManager, confirmationDialogHandler, stopShowingDelegate);
        OneshotSupplierImpl<PropertyModel> modelSupplier = new OneshotSupplierImpl<>();
        View buttonBarView =
                createCustomButtonBarView(mContext, modelSupplier, positiveText, negativeText);
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dismissHandler)
                        .with(ModalDialogProperties.TITLE, titleText)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, buttonBarView)
                        .build();
        modelSupplier.set(model);
        dismissHandler.setPropertyModel(model);
        mModalDialogManager.showDialog(model, ModalDialogType.APP);
    }

    private static View createCustomButtonBarView(
            Context context,
            OneshotSupplier<PropertyModel> modelSupplier,
            String positiveText,
            String negativeText) {
        SpinnerButtonWrapper positiveButtonSpinner =
                createSpinnerButton(
                        context, modelSupplier, /* isPositiveButton= */ true, positiveText);
        SpinnerButtonWrapper negativeButtonSpinner =
                createSpinnerButton(
                        context, modelSupplier, /* isPositiveButton= */ false, negativeText);
        return ModalDialogViewUtils.createCustomButtonBarView(
                context, positiveButtonSpinner, negativeButtonSpinner);
    }

    private static SpinnerButtonWrapper createSpinnerButton(
            Context context,
            OneshotSupplier<PropertyModel> modelSupplier,
            boolean isPositiveButton,
            String buttonText) {
        int layoutButtonType =
                isPositiveButton
                        ? DualControlLayout.ButtonType.PRIMARY_FILLED
                        : DualControlLayout.ButtonType.SECONDARY_TEXT;
        @ColorInt
        int spinnerColor =
                isPositiveButton
                        ? SemanticColorUtils.getDefaultBgColor(context)
                        : SemanticColorUtils.getDefaultIconColorAccent1(context);
        int dialogButtonType =
                isPositiveButton
                        ? ModalDialogProperties.ButtonType.POSITIVE
                        : ModalDialogProperties.ButtonType.NEGATIVE;

        Button button =
                DualControlLayout.createButtonForLayout(
                        context, layoutButtonType, buttonText, null);
        SpinnerButtonWrapper spinnerButtonWrapper =
                SpinnerButtonWrapper.createSpinnerButtonWrapper(
                        context,
                        button,
                        R.string.collaboration_loading_button,
                        R.dimen.modal_dialog_spinner_size,
                        spinnerColor,
                        () -> {
                            // There is no need to use #onAvailable() for the modelSupplier as the
                            // model will always be readily available since the use of a {@link
                            // OneshotSupplier} is to resolve a dependency ordering issue.
                            PropertyModel model = modelSupplier.get();
                            model.get(ModalDialogProperties.CONTROLLER)
                                    .onClick(model, dialogButtonType);
                        });
        return spinnerButtonWrapper;
    }
}
