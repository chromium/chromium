// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.payments.InputProtector;
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
    private Runnable mOptOutCallback;
    private SecurePaymentConfirmationNoMatchingCredView mView;

    private InputProtector mInputProtector = new InputProtector();

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    switch (newState) {
                        case BottomSheetController.SheetState.HIDDEN:
                            close();
                            break;
                    }
                }
            };

    private final BottomSheetContent mBottomSheetContent =
            new BottomSheetContent() {
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
                    return R.string
                            .secure_payment_confirmation_no_matching_credential_sheet_description;
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

    /** Closes the SPC No Matching Credential UI. */
    public void close() {
        if (mResponseCallback != null) {
            mResponseCallback.run();
            mResponseCallback = null;
        }

        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    public void closePressed() {
        if (mInputProtector.shouldInputBeProcessed()) close();
    }

    public void optOut() {
        assert mOptOutCallback != null;
        mOptOutCallback.run();

        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    public void optOutPressed() {
        if (mInputProtector.shouldInputBeProcessed()) optOut();
    }

    /**
     * Shows the SPC No Matching Credential UI.
     *
     * @param responseCallback Invoked when users respond to the UI.
     * @param optOutCallback Invoked if the user elects to opt out on the UI.
     * @param showOptOut Whether to display the opt out UX to the user.
     * @param rpId The relying party ID of the SPC credential.
     * @return whether or not the UI was successfully shown.
     */
    public boolean show(
            Runnable responseCallback, Runnable optOutCallback, boolean showOptOut, String rpId) {
        if (mHider != null) return false;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return false;
        Context context = windowAndroid.getContext().get();
        if (context == null) return false;

        BottomSheetController bottomSheet = BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheet == null) return false;

        mInputProtector.markShowTime();

        bottomSheet.addObserver(mBottomSheetObserver);

        String origin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mWebContents.getVisibleUrl().getOrigin().getSpec(),
                        SchemeDisplay.OMIT_CRYPTOGRAPHIC);

        mView =
                new SecurePaymentConfirmationNoMatchingCredView(
                        context, origin, rpId, showOptOut, this::closePressed, this::optOutPressed);

        mHider =
                () -> {
                    bottomSheet.removeObserver(mBottomSheetObserver);
                    bottomSheet.hideContent(
                            /* content= */ mBottomSheetContent, /* animate= */ true);
                };

        mResponseCallback = responseCallback;
        mOptOutCallback = showOptOut ? optOutCallback : null;

        if (!bottomSheet.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            close();
            return false;
        }
        return true;
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public SecurePaymentConfirmationNoMatchingCredView getView() {
        return mView;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean isHidden() {
        return mHider == null;
    }

    /**
     * Called by PaymentRequestTestBridge for cross-platform browsertests, the following methods
     * bypass the input protector. The Java unit tests simulate clicking the button and therefore
     * test the input protector.
     */
    public boolean optOutForTest() {
        if (mOptOutCallback == null) return false;
        optOut();
        return true;
    }

    public boolean closeForTest() {
        close();
        return true;
    }
}
