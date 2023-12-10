// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.subresource_filter;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ClickableSpan;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;

/**
 * Java part of AdsBlockedDialog pair providing communication between native ads blocked
 * delegate code and Java ads blocked dialog UI components.
 */
public class AdsBlockedDialog implements ModalDialogProperties.Controller {
    private long mNativeDialog;
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private PropertyModel mDialogModel;
    private ClickableSpan mClickableSpan;
    private Handler mDialogHandler;

    @CalledByNative
    static AdsBlockedDialog create(long nativeDialog, @NonNull WindowAndroid windowAndroid) {
        return new AdsBlockedDialog(nativeDialog, windowAndroid);
    }

    AdsBlockedDialog(long nativeDialog, @NonNull WindowAndroid windowAndroid) {
        mNativeDialog = nativeDialog;
        mContext = windowAndroid.getContext().get();
        mModalDialogManager = windowAndroid.getModalDialogManager();
        mDialogHandler = new Handler(ThreadUtils.getUiThreadLooper());
    }

    /**
     * Internal constructor for {@link AdsBlockedDialog}. Used by tests to inject
     * parameters. External code should use AdsBlockedDialog#create.
     *
     * @param nativeDialog The pointer to the dialog instance created by native code.
     * @param context The context for accessing resources.
     * @param modalDialogManager The ModalDialogManager to display the dialog.
     * @param dialogHandler The {@link Handler} used to post the call to show the dialog.
     */
    @VisibleForTesting
    AdsBlockedDialog(
            long nativeDialog,
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            Handler dialogHandler) {
        mNativeDialog = nativeDialog;
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDialogHandler = dialogHandler;
    }

    PropertyModel getDialogModelForTesting() {
        return mDialogModel;
    }

    ClickableSpan getMessageClickableSpanForTesting() {
        return mClickableSpan;
    }

    @CalledByNative
    void show(boolean shouldPostDialog) {
        Resources resources = mContext.getResources();
        mClickableSpan =
                new NoUnderlineClickableSpan(
                        mContext,
                        (view) -> AdsBlockedDialogJni.get().onLearnMoreClicked(mNativeDialog));
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                R.string.blocked_ads_dialog_title)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, getFormattedMessageText())
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.blocked_ads_dialog_always_allow)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.FOCUS_DIALOG, true)
                        .build();

        // shouldPostDialog determines if ModalDialogManager#showDialog should be invoked directly
        // or using Handler#post.
        // The dialog should be re-shown on the original tab on navigation back from the redirected
        // support link tab. This redirection is observed by
        // WebContentsObserver#OnWebContentsFocused and
        // TabModelSelectorTabModelObserver#didSelectTab. The sequence of invocation of these
        // methods is not consistent on phones and tablets. Using #post will delay the
        // #showDialog request reliably to after both tab events are handled on the UI thread,
        // guaranteeing that the dialog will be re-shown as expected. See crbug.com/1261967 for
        // details.
        // TODO (crbug.com/1272049): Investigate tab event observer ordering and tab modal
        // suspension logic as a follow up.
        if (shouldPostDialog) {
            mDialogHandler.post(
                    () -> mModalDialogManager.showDialog(mDialogModel, ModalDialogType.TAB));
        } else {
            mModalDialogManager.showDialog(mDialogModel, ModalDialogType.TAB);
        }
    }

    @CalledByNative
    void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    // Returns link-formatted message text for the ads blocked dialog.
    @VisibleForTesting
    CharSequence getFormattedMessageText() {
        Resources resources = mContext.getResources();
        String messageText = resources.getString(R.string.blocked_ads_dialog_message);
        String learnMoreLinkText = resources.getString(R.string.blocked_ads_dialog_learn_more);
        final SpannableString formattedLinkText = new SpannableString(learnMoreLinkText);
        formattedLinkText.setSpan(
                mClickableSpan, 0, learnMoreLinkText.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        return TextUtils.expandTemplate(messageText, formattedLinkText);
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        assert mNativeDialog != 0;
        if (buttonType == ButtonType.POSITIVE) {
            AdsBlockedDialogJni.get().onAllowAdsClicked(mNativeDialog);
        }
        mModalDialogManager.dismissDialog(
                model,
                buttonType == ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogHandler.removeCallbacksAndMessages(null);
        AdsBlockedDialogJni.get().onDismissed(mNativeDialog);
        mNativeDialog = 0;
    }

    @NativeMethods
    interface Natives {
        void onAllowAdsClicked(long nativeAdsBlockedDialog);

        void onLearnMoreClicked(long nativeAdsBlockedDialog);

        void onDismissed(long nativeAdsBlockedDialog);
    }
}
