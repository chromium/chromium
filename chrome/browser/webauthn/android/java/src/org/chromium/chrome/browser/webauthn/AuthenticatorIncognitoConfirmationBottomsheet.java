// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.ScrollView;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bottomsheet to confirm credential creation in Incognito mode.
 *
 * <p>This class shows a bottomsheet on Android that warns the user that they are about to create a
 * WebAuthn credential in Incognito mode (because the credential will outlive the Incognito
 * session.)
 *
 * <p>If the user clicks "Continue" the `positiveCallback` is run. If the user closes the
 * bottomsheet in any other way, the `negativeCallback` is run.
 */
class AuthenticatorIncognitoConfirmationBottomsheet {
    private final WebContents mWebContents;
    private BottomSheetController mController;
    private Runnable mPositiveCallback;
    private Runnable mNegativeCallback;
    private ScrollView mScrollView;
    @VisibleForTesting RelativeLayout mContentView;
    @VisibleForTesting boolean mIsShowing;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    if (newState == BottomSheetController.SheetState.HIDDEN) {
                        close(false);
                    }
                }
            };

    private final BottomSheetContent mBottomSheetContent =
            new BottomSheetContent() {
                @Override
                public View getContentView() {
                    return mContentView;
                }

                @Override
                public View getToolbarView() {
                    return null;
                }

                @Override
                public int getVerticalScrollOffset() {
                    if (mScrollView != null) {
                        return mScrollView.getScrollY();
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
                    return R.string.webauthn_incognito_confirmation_sheet_description;
                }

                @Override
                public int getSheetHalfHeightAccessibilityStringId() {
                    assert false : "This method should not be called";
                    return 0;
                }

                @Override
                public int getSheetFullHeightAccessibilityStringId() {
                    return R.string.webauthn_incognito_confirmation_sheet_opened;
                }

                @Override
                public int getSheetClosedAccessibilityStringId() {
                    return R.string.webauthn_incognito_confirmation_sheet_closed;
                }
            };

    public AuthenticatorIncognitoConfirmationBottomsheet(WebContents webContents) {
        mWebContents = webContents;
    }

    public void close(boolean success) {
        if (!mIsShowing) return;

        mController.removeObserver(mBottomSheetObserver);
        mController.hideContent(/* content= */ mBottomSheetContent, /* animate= */ true);
        mIsShowing = false;

        if (success) {
            mPositiveCallback.run();
        } else {
            mNegativeCallback.run();
        }
    }

    public boolean show(Runnable positiveCallback, Runnable negativeCallback) {
        assert positiveCallback != null;
        assert negativeCallback != null;
        assert !mIsShowing;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return false;
        if (mController == null) {
            mController = BottomSheetControllerProvider.from(windowAndroid);
        }
        if (mController == null) return false;
        Context context = windowAndroid.getContext().get();
        if (context == null) return false;

        mController.addObserver(mBottomSheetObserver);

        mPositiveCallback = positiveCallback;
        mNegativeCallback = negativeCallback;

        createView(context);

        mIsShowing = true;
        if (!mController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            // close will call one of the callbacks. In the event that we weren't able to show the
            // confirmation sheet, we continue with the creation since, historically, we didn't have
            // a confirmation sheet. Also, a bug that make it impossible to create credentials in
            // Incognito would be bad.
            close(true);
            // The return value needs to be true since one of the callbacks was called.
        }
        return true;
    }

    private void createView(Context context) {
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.webauthn_incognito_confirmation, null);
        mScrollView = (ScrollView) mContentView.findViewById(R.id.scroll_view);

        ((Button) mContentView.findViewById(R.id.continue_button))
                .setOnClickListener((v) -> close(true));
        ((Button) mContentView.findViewById(R.id.cancel_button))
                .setOnClickListener((v) -> close(false));
    }
}
