// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller that allows the native autofill code to show a progress bar (spinner). For example:
 * When unmasking a virtual card, we show a progress bar while we contact the bank.
 *
 * <p>Note: The progress bar dialog only shows a negative button which dismisses the dialog.
 */
@JNINamespace("autofill")
@NullMarked
public class AutofillProgressDialogBridge {
    private static final int SUCCESS_VIEW_DURATION_MILLIS = 500;

    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private long mNativeAutofillProgressDialogView;
    private @Nullable PropertyModel mDialogModel;
    private @Nullable View mProgressDialogContentView;

    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    mModalDialogManager.dismissDialog(
                            mDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    if (mNativeAutofillProgressDialogView != 0) {
                        AutofillProgressDialogBridgeJni.get()
                                .onDismissed(mNativeAutofillProgressDialogView);
                    }
                }
            };

    public AutofillProgressDialogBridge(
            long nativeAutofillProgressDialogView,
            ModalDialogManager modalDialogManager,
            Context context) {
        this.mNativeAutofillProgressDialogView = nativeAutofillProgressDialogView;
        this.mModalDialogManager = modalDialogManager;
        this.mContext = context;
    }

    @CalledByNative
    public static AutofillProgressDialogBridge create(
            long nativeAutofillProgressDialogView, WindowAndroid windowAndroid) {
        return new AutofillProgressDialogBridge(
                nativeAutofillProgressDialogView,
                assertNonNull(windowAndroid.getModalDialogManager()),
                assertNonNull(windowAndroid.getActivity().get()));
    }

    /**
     * Shows a progress bar dialog.
     *
     * @param loadingMessage Message to show below the progress bar.
     */
    @CalledByNative
    public void showDialog(String title, String loadingMessage, String buttonLabel) {
        mProgressDialogContentView =
                LayoutInflater.from(mContext).inflate(R.layout.autofill_progress_dialog, null);
        ((TextView) mProgressDialogContentView.findViewById(R.id.message)).setText(loadingMessage);

        ViewStub title_view_stub =
                mProgressDialogContentView.findViewById(R.id.title_with_icon_stub);
        title_view_stub.setLayoutResource(R.layout.icon_after_title_view);
        title_view_stub.inflate();
        TextView titleView = mProgressDialogContentView.findViewById(R.id.title);
        titleView.setText(title);
        ImageView iconView = mProgressDialogContentView.findViewById(R.id.title_icon);
        iconView.setImageResource(R.drawable.google_pay);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mProgressDialogContentView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, buttonLabel);
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Replaces the progress bar and loadingMessage with a confirmation icon and message and then
     * dismisses the dialog after a certain period of time. NOTE: This should only be called after
     * show(~) has been called.
     *
     * @param confirmationMessage Message to show below the confirmation icon
     */
    @CalledByNative
    public void showConfirmation(String confirmationMessage) {
        if (mProgressDialogContentView != null) {
            mProgressDialogContentView.findViewById(R.id.progress_bar).setVisibility(View.GONE);
            mProgressDialogContentView
                    .findViewById(R.id.confirmation_icon)
                    .setVisibility(View.VISIBLE);
            TextView confirmationMessageView =
                    mProgressDialogContentView.findViewById(R.id.message);
            confirmationMessageView.setAccessibilityLiveRegion(
                    View.ACCESSIBILITY_LIVE_REGION_POLITE);
            confirmationMessageView.setText(confirmationMessage);
            // TODO(crbug.com/40195445): Dismiss the Java View after some delay if confirmation has
            // been shown.
        }
        Runnable dismissRunnable = () -> dismiss();
        new Handler().postDelayed(dismissRunnable, SUCCESS_VIEW_DURATION_MILLIS);
    }

    /** Dismisses the currently showing dialog. */
    @CalledByNative
    public void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @CalledByNative
    public void destroy() {
        mNativeAutofillProgressDialogView = 0;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeAutofillProgressDialogViewAndroid);
    }
}
