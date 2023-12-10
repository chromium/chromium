// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
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
 * Controller that allows the native autofill code to show an error dialog.
 * For example: When unmasking a virtual card returns an error, we show an error dialog with more
 * information about the error.
 *
 * Note: The error dialog only shows a positive button which dismisses the dialog.
 */
@JNINamespace("autofill")
public class AutofillErrorDialogBridge {
    private final long mNativeAutofillErrorDialogView;
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private PropertyModel mDialogModel;

    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    mModalDialogManager.dismissDialog(
                            mDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    AutofillErrorDialogBridgeJni.get().onDismissed(mNativeAutofillErrorDialogView);
                }
            };

    public AutofillErrorDialogBridge(
            long nativeAutofillErrorDialogView,
            ModalDialogManager modalDialogManager,
            Context context) {
        this.mNativeAutofillErrorDialogView = nativeAutofillErrorDialogView;
        this.mModalDialogManager = modalDialogManager;
        this.mContext = context;
    }

    @CalledByNative
    public static AutofillErrorDialogBridge create(
            long nativeAutofillErrorDialogView, WindowAndroid windowAndroid) {
        return new AutofillErrorDialogBridge(
                nativeAutofillErrorDialogView,
                windowAndroid.getModalDialogManager(),
                windowAndroid.getActivity().get());
    }

    /**
     * Shows an error dialog.
     *
     * @param title Title for the error dialog.
     * @param description Description for the error dialog which shows below the title.
     * @param buttonLabel Label for the positive button which acts as a cancel button.
     * @param titleIconId The resource id for the icon to be displayed to the left of the title. If
     * flag 'AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK' is enabled titleIconId is
     * overridden.
     */
    @CalledByNative
    public void show(String title, String description, String buttonLabel, int titleIconId) {
        View errorDialogContentView =
                LayoutInflater.from(mContext).inflate(R.layout.autofill_error_dialog, null);
        ((TextView) errorDialogContentView.findViewById(R.id.error_message)).setText(description);

        boolean useCustomTitleView =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK);
        if (useCustomTitleView) {
            ViewStub stub = errorDialogContentView.findViewById(R.id.title_with_icon_stub);
            stub.setLayoutResource(R.layout.icon_after_title_view);
            stub.inflate();
            titleIconId = R.drawable.google_pay;
        }
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, errorDialogContentView)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, buttonLabel);
        updateTitleView(useCustomTitleView, title, titleIconId, builder, errorDialogContentView);
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
     * @param errorDialogContentView The CUSTOM_VIEW used in the PropertyModel that also has the
     * view stub for custom title view.
     */
    private void updateTitleView(
            boolean useCustomTitleView,
            String title,
            @DrawableRes int titleIcon,
            PropertyModel.Builder builder,
            View errorDialogContentView) {
        if (useCustomTitleView) {
            TextView titleView = (TextView) errorDialogContentView.findViewById(R.id.title);
            titleView.setText(title);
            ImageView iconView = (ImageView) errorDialogContentView.findViewById(R.id.title_icon);
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

    /** Dismisses the currently showing dialog. */
    @CalledByNative
    public void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeAutofillErrorDialogViewAndroid);
    }
}
