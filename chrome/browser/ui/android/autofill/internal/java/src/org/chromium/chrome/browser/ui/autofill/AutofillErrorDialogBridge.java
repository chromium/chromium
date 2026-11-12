// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller that allows the native autofill code to show an error dialog. For example: When
 * unmasking a virtual card returns an error, we show an error dialog with more information about
 * the error.
 *
 * <p>Note: The error dialog only shows a positive button which dismisses the dialog.
 */
@JNINamespace("autofill")
@NullMarked
public class AutofillErrorDialogBridge {
    @JniType("::autofill::AutofillErrorDialogViewAndroid")
    interface NativeAutofillErrorDialogViewAndroid extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeAutofillErrorDialogViewAndroid>
            mNativeAutofillErrorDialogView;
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private @Nullable PropertyModel mDialogModel;

    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    mModalDialogManager.dismissDialog(
                            mDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    if (mNativeAutofillErrorDialogView != null) {
                        AutofillErrorDialogBridgeJni.get()
                                .onDismissed(mNativeAutofillErrorDialogView);
                        clearNativePtr();
                    }
                }
            };

    public AutofillErrorDialogBridge(
            @Nullable JniRawPtr<NativeAutofillErrorDialogViewAndroid> nativeAutofillErrorDialogView,
            ModalDialogManager modalDialogManager,
            Context context) {
        this.mNativeAutofillErrorDialogView = nativeAutofillErrorDialogView;
        this.mModalDialogManager = modalDialogManager;
        this.mContext = context;
    }

    @CalledByNative
    public static AutofillErrorDialogBridge create(
            JniRawPtr<NativeAutofillErrorDialogViewAndroid> nativeAutofillErrorDialogView,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid) {
        return new AutofillErrorDialogBridge(
                nativeAutofillErrorDialogView,
                assertNonNull(windowAndroid.getModalDialogManager()),
                assertNonNull(windowAndroid.getActivity().get()));
    }

    /**
     * Shows an error dialog.
     *
     * @param title Title for the error dialog.
     * @param description Description for the error dialog which shows below the title.
     * @param buttonLabel Label for the positive button which acts as a cancel button.
     */
    @CalledByNative
    public void show(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String description,
            @JniType("std::u16string") String buttonLabel) {
        View errorDialogContentView =
                LayoutInflater.from(mContext).inflate(R.layout.autofill_error_dialog, null);
        ((TextView) errorDialogContentView.findViewById(R.id.error_message)).setText(description);

        ViewStub titleViewStub = errorDialogContentView.findViewById(R.id.title_with_icon_stub);
        titleViewStub.setLayoutResource(R.layout.icon_after_title_view);
        titleViewStub.inflate();
        TextView titleView = errorDialogContentView.findViewById(R.id.title);
        titleView.setText(title);
        ImageView iconView = errorDialogContentView.findViewById(R.id.title_icon);
        iconView.setImageResource(
                ChromeFeatureList.isEnabled(AutofillFeatures.AUTOFILL_ENABLE_GRADIENT_GOOGLE_LOGOS)
                        ? R.drawable.googlepay_with_gradient
                        : R.drawable.google_pay);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, errorDialogContentView)
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, title)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, buttonLabel);
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /** Dismisses the currently showing dialog. */
    @CalledByNative
    public void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @CalledByNative
    void clearNativePtr() {
        if (mNativeAutofillErrorDialogView != null) {
            mNativeAutofillErrorDialogView.release();
            mNativeAutofillErrorDialogView = null;
        }
    }

    @NativeMethods
    interface Natives {
        void onDismissed(JniPtr<NativeAutofillErrorDialogViewAndroid> self);
    }
}
