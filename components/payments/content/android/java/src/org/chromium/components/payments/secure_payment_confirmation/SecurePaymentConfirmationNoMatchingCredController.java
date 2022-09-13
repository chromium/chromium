// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.view.View;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.payments.R;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * The controller of the SecurePaymentConfirmation No Matching Credential UI, which owns the
 * component overall, i.e., creates other objects in the component and connects them. It decouples
 * the implementation of this component from other components and acts as the point of contact
 * between them. Any code in this component that needs to interact with another component does that
 * through this controller.
 */
public class SecurePaymentConfirmationNoMatchingCredController {
    private final WebContents mWebContents;
    private Runnable mHider;
    private Runnable mResponseCallback;
    private SecurePaymentConfirmationNoMatchingCredView mView;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(int newState, int reason) {
            switch (newState) {
                case BottomSheetController.SheetState.HIDDEN:
                    hide();
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
            if (mView != null) {
                return mView.getScrollY();
            }

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
            return R.string.secure_payment_confirmation_no_matching_credential_sheet_description;
        }

        @Override
        public int getSheetHalfHeightAccessibilityStringId() {
            assert false : "This method should not be called";
            return 0;
        }

        @Override
        public int getSheetFullHeightAccessibilityStringId() {
            return R.string.secure_payment_confirmation_no_matching_credential_sheet_opened;
        }

        @Override
        public int getSheetClosedAccessibilityStringId() {
            return R.string.secure_payment_confirmation_no_matching_credential_sheet_closed;
        }
    };

    /**
     * Constructs the SPC No Matching Credential UI controller.
     *
     * @param webContents The WebContents of the merchant.
     */
    public static SecurePaymentConfirmationNoMatchingCredController create(
            WebContents webContents) {
        return webContents != null
                ? new SecurePaymentConfirmationNoMatchingCredController(webContents)
                : null;
    }

    private SecurePaymentConfirmationNoMatchingCredController(WebContents webContents) {
        mWebContents = webContents;
    }

    /** Hides the SPC No Matching Credential UI. */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    /**
     * Shows the SPC No Matching Credential UI.
     *
     * @param callback Invoked when users respond to the UI.
     */
    public void show(Runnable callback) {
        if (mHider != null) return;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        Context context = windowAndroid.getContext().get();
        if (context == null) return;

        BottomSheetController bottomSheet = BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheet == null) return;

        bottomSheet.addObserver(mBottomSheetObserver);

        String origin = UrlFormatter.formatUrlForSecurityDisplay(
                mWebContents.getVisibleUrl().getOrigin().getSpec(),
                SchemeDisplay.OMIT_CRYPTOGRAPHIC);

        mView = new SecurePaymentConfirmationNoMatchingCredView(context, origin, this::hide);

        mHider = () -> {
            if (mResponseCallback != null) {
                mResponseCallback.run();
                mResponseCallback = null;
            }
            bottomSheet.removeObserver(mBottomSheetObserver);
            bottomSheet.hideContent(/*content=*/mBottomSheetContent, /*animate=*/true);
        };

        mResponseCallback = callback;

        if (!bottomSheet.requestShowContent(mBottomSheetContent, /*animate=*/true)) hide();
    }
}
