// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.payments.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * PaymentCredentialEnrollment coordinator for Android, which owns the component overall,
 * i.e., creates other objects in the component and connects them. It decouples the implementation
 * of this component from other components and acts as the point of contact between them. Any code
 * in this component that needs to interact with another component does that through this
 * coordinator.
 */
public class PaymentCredentialEnrollmentController extends WebContentsObserver {
    private final WebContents mWebContents;
    private Runnable mHider;
    private Callback<Boolean> mResponseCallback;
    private PaymentCredentialEnrollmentView mView;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(int newState, int reason) {
            switch (newState) {
                case BottomSheetController.SheetState.HIDDEN:
                    onCancel();
                    break;
            }
        }
    };

    private final BottomSheetContent mBottomSheetContent = new BottomSheetContent() {
        @Override
        public View getContentView() {
            return mView.getContentView();
        }

        @Override
        public View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            if (mView != null) return mView.getScrollY();
            return 0;
        }

        @Override
        public float getFullHeightRatio() {
            return HeightMode.WRAP_CONTENT;
        }

        @Override
        public float getHalfHeightRatio() {
            return HeightMode.DISABLED;
        }

        @Override
        public void destroy() {}

        @Override
        public int getPriority() {
            return ContentPriority.HIGH;
        }

        @Override
        public int getPeekHeight() {
            return HeightMode.DISABLED;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return false;
        }

        @Override
        public int getSheetContentDescriptionStringId() {
            return R.string.secure_payment_confirmation_enrollment_sheet_description;
        }

        @Override
        public int getSheetHalfHeightAccessibilityStringId() {
            assert false : "This method should not be called";
            return 0;
        }

        @Override
        public int getSheetFullHeightAccessibilityStringId() {
            return R.string.secure_payment_confirmation_enrollment_sheet_opened;
        }

        @Override
        public int getSheetClosedAccessibilityStringId() {
            return R.string.secure_payment_confirmation_enrollment_sheet_closed;
        }
    };

    /**
     * Constructs the SPC Enrollment UI component controller.
     *
     * @param webContents The WebContents of the merchant.
     */
    public static PaymentCredentialEnrollmentController create(WebContents webContents) {
        return webContents != null ? new PaymentCredentialEnrollmentController(webContents) : null;
    }

    private PaymentCredentialEnrollmentController(WebContents webContents) {
        mWebContents = webContents;
    }

    /**
     * Shows the SPC Enrollment UI.
     *
     * @param callback Invoked when users respond to the UI. The callback result is true when the
     *        user accepts the enrollment, false when the user dismisses
     * @param instrumentName The name of the payment instrument.
     * @param icon The icon of the payment instrument.
     * @param isIncognito Whether the tab is currently in incognito mode.
     */
    public boolean show(
            Callback<Boolean> callback, String instrumentName, Bitmap icon, boolean isIncognito) {
        if (mHider != null) return false;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return false;

        Context context = windowAndroid.getContext().get();
        if (context == null) return false;

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return false;

        Drawable drawableIcon = new BitmapDrawable(context.getResources(), icon);
        PropertyModel mModel =
                new PropertyModel.Builder(PaymentCredentialEnrollmentProperties.ALL_KEYS)
                        .with(PaymentCredentialEnrollmentProperties.PAYMENT_ICON, drawableIcon)
                        .with(PaymentCredentialEnrollmentProperties.PAYMENT_INSTRUMENT_LABEL,
                                instrumentName)
                        .with(PaymentCredentialEnrollmentProperties.CONTINUE_BUTTON_CALLBACK,
                                this::onContinue)
                        .with(PaymentCredentialEnrollmentProperties.CANCEL_BUTTON_CALLBACK,
                                this::onCancel)
                        .with(PaymentCredentialEnrollmentProperties.INCOGNITO_TEXT_VISIBLE,
                                isIncognito)
                        .build();

        bottomSheetController.addObserver(mBottomSheetObserver);

        mView = new PaymentCredentialEnrollmentView(context);
        PropertyModelChangeProcessor changeProcessor = PropertyModelChangeProcessor.create(
                mModel, mView, PaymentCredentialEnrollmentViewBinder::bind);

        mHider = () -> {
            changeProcessor.destroy();
            bottomSheetController.removeObserver(mBottomSheetObserver);
            bottomSheetController.hideContent(/*content=*/mBottomSheetContent, /*animate=*/true);
        };

        mResponseCallback = callback;

        boolean isShowSuccess =
                bottomSheetController.requestShowContent(mBottomSheetContent, /*animate=*/true);
        if (!isShowSuccess) {
            hide();
            return false;
        }

        return true;
    }

    /**
     * Hides the SPC Enrollment UI.
     */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    private void onContinue() {
        hide();
        mResponseCallback.onResult(true);
    }

    private void onCancel() {
        hide();
        mResponseCallback.onResult(false);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public PaymentCredentialEnrollmentView getView() {
        return mView;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean isHidden() {
        return mHider == null;
    }
}