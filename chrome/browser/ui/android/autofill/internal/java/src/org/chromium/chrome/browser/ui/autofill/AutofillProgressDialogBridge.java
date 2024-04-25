// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.core.content.res.ResourcesCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller that allows the native autofill code to show a progress bar (spinner).
 * For example: When unmasking a virtual card, we show a progress bar while
 * we contact the bank.
 *
 * Note: The progress bar dialog only shows a negative button which dismisses the dialog.
 */
@JNINamespace("autofill")
public class AutofillProgressDialogBridge {
    private static final int SUCCESS_VIEW_DURATION_MILLIS = 500;

    private final long mNativeAutofillProgressDialogView;
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private PropertyModel mDialogModel;
    private View mProgressDialogContentView;

    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    mModalDialogManager.dismissDialog(
                            mDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    AutofillProgressDialogBridgeJni.get()
                            .onDismissed(mNativeAutofillProgressDialogView);
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
                windowAndroid.getModalDialogManager(),
                windowAndroid.getActivity().get());
    }

    /**
     * Shows a progress bar dialog.
     *
     * @param loadingMessage Message to show below the progress bar.
     * @param titleIconId The resource id for the icon to be displayed to the left of the title. If
     * flag 'AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK' is enabled titleIconId is
     * overridden.
     */
    @CalledByNative
    public void showDialog(
            String title, String loadingMessage, String buttonLabel, int titleIconId) {
        mProgressDialogContentView =
                LayoutInflater.from(mContext).inflate(R.layout.autofill_progress_dialog, null);
        ((TextView) mProgressDialogContentView.findViewById(R.id.message)).setText(loadingMessage);

        boolean useCustomTitleView =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK);

        if (useCustomTitleView) {
            ViewStub stub = mProgressDialogContentView.findViewById(R.id.title_with_icon_stub);
            stub.setLayoutResource(R.layout.icon_after_title_view);
            stub.inflate();
            titleIconId = R.drawable.google_pay;
        }

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mProgressDialogContentView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, buttonLabel);
        updateTitleView(useCustomTitleView, title, titleIconId, builder);
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Updates the title and icon view. If AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK
     * feature is enabled, sets title and icon in the customView otherwise uses
     * PropertyModel.Builder for title and icon.
     *
     * @param useCustomTitleView Indicates true/false to use custom title view.
     * @param title Title of the prompt dialog.
     * @param titleIcon Icon near the title.
     * @param builder The PropertyModel.Builder instance.
     */
    private void updateTitleView(
            boolean useCustomTitleView,
            String title,
            @DrawableRes int titleIcon,
            PropertyModel.Builder builder) {
        if (useCustomTitleView) {
            TextView titleView = (TextView) mProgressDialogContentView.findViewById(R.id.title);
            titleView.setText(title);
            ImageView iconView =
                    (ImageView) mProgressDialogContentView.findViewById(R.id.title_icon);
            iconView.setImageResource(titleIcon);
        } else {
            builder.with(ModalDialogProperties.TITLE, title);
            if (titleIcon != 0) {
                builder.with(
                        ModalDialogProperties.TITLE_ICON,
                        ResourcesCompat.getDrawable(
                                mContext.getResources(), titleIcon, mContext.getTheme()));
            }
        }
    }

    /**
     * Replaces the progress bar and loadingMessage with a confirmation icon and message and then
     * dismisses the dialog after a certain period of time.
     * NOTE: This should only be called after show(~) has been called.
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
            ((TextView) mProgressDialogContentView.findViewById(R.id.message))
                    .setText(confirmationMessage);
            mProgressDialogContentView.announceForAccessibility(confirmationMessage);
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

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeAutofillProgressDialogViewAndroid);
    }
}
