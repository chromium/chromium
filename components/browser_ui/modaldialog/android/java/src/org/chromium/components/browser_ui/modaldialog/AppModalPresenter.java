// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager;

import androidx.activity.ComponentDialog;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.StrictModeContext;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The presenter that shows a {@link ModalDialogView} in an Android dialog. */
public class AppModalPresenter extends ModalDialogManager.Presenter {
    // Duration of enter animation. This is an estimation because there is no reliable way to
    // get duration of AlertDialog's enter animation.
    private static final long ENTER_ANIMATION_ESTIMATION_MS = 200;
    private final Context mContext;
    private ComponentDialog mDialog;
    private PropertyModelChangeProcessor<PropertyModel, ModalDialogView, PropertyKey>
            mModelChangeProcessor;

    private class ViewBinder extends ModalDialogViewBinder {
        @Override
        public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
            if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
                mDialog.setCanceledOnTouchOutside(
                        model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
            } else if (ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER == propertyKey) {
                mDialog.getOnBackPressedDispatcher()
                        .addCallback(
                                model.get(
                                        ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER));
            } else {
                super.bind(model, view, propertyKey);
            }
        }
    }

    /** @param activity The {@link Activity} on which dialog views will be created and shown. */
    public AppModalPresenter(Context context) {
        mContext = context;
    }

    private ModalDialogView loadDialogView() {
        return (ModalDialogView)
                LayoutInflaterUtils.inflate(mDialog.getContext(), R.layout.modal_dialog_view, null);
    }

    @Override
    protected void addDialogView(PropertyModel model) {
        int styles[][] = {
            {
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton_Fullscreen,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton_DialogWhenLarge,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton_Fullscreen_Dark
            },
            {
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton_Fullscreen,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton_DialogWhenLarge,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton_Fullscreen_Dark
            },
            {
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton_Fullscreen,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton_DialogWhenLarge,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton_Fullscreen_Dark
            }
        };
        int dialogIndex = 0;
        int dialogStyle = model.get(ModalDialogProperties.DIALOG_STYLES);
        if (dialogStyle == ModalDialogProperties.DialogStyles.FULLSCREEN_DIALOG) {
            dialogIndex = 1;
        } else if (dialogStyle == ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE) {
            dialogIndex = 2;
        } else if (dialogStyle == ModalDialogProperties.DialogStyles.FULLSCREEN_DARK_DIALOG) {
            dialogIndex = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ? 3 : 1;
        }
        int buttonIndex = 0;
        int buttonStyle = model.get(ModalDialogProperties.BUTTON_STYLES);
        if (buttonStyle == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE) {
            buttonIndex = 1;
        } else if (buttonStyle
                == ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED) {
            buttonIndex = 2;
        }
        mDialog = new ComponentDialog(mContext, styles[buttonIndex][dialogIndex]);
        mDialog.setOnCancelListener(
                dialogInterface ->
                        dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));

        // Cancel on touch outside should be disabled by default. The ModelChangeProcessor wouldn't
        // notify change if the property is not set during initialization.
        mDialog.setCanceledOnTouchOutside(false);
        ModalDialogView dialogView = loadDialogView();
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(model, dialogView, new ViewBinder());
        // setContentView() can trigger using LayoutInflater, which may read from disk.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mDialog.setContentView(dialogView);
        }

        mDialog.setOnShowListener(
                (dialogInterface) -> {
                    dialogView.onEnterAnimationStarted(ENTER_ANIMATION_ESTIMATION_MS);
                });

        try {
            mDialog.show();
        } catch (WindowManager.BadTokenException badToken) {
            // See https://crbug.com/926688.
            dismissCurrentDialog(DialogDismissalCause.NOT_ATTACHED_TO_WINDOW);
        }
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }

        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
    }

    @VisibleForTesting
    public Window getWindow() {
        return mDialog.getWindow();
    }
}
