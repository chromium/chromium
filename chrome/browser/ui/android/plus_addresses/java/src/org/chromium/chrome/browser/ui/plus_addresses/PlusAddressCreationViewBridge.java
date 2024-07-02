// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.layouts.LayoutManagerProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** JNI wrapper for C++ PlusAddressCreationViewAndroid. */
@JNINamespace("plus_addresses")
public class PlusAddressCreationViewBridge {
    private long mNativePlusAddressCreationPromptAndroid;
    private Activity mActivity;
    private BottomSheetController mBottomSheetController;
    private LayoutStateProvider mLayoutStateProvider;
    private final TabModel mTabModel;
    private final TabModelSelector mTabModelSelector;
    private CoordinatorFactory mCoordinatorFactory;
    @Nullable private PlusAddressCreationCoordinator mCoordinator;

    @VisibleForTesting
    /*package*/ PlusAddressCreationViewBridge(
            long nativePlusAddressCreationPromptAndroid,
            WindowAndroid window,
            TabModel tabModel,
            TabModelSelector tabModelSelector,
            CoordinatorFactory coordinatorFactory) {
        mNativePlusAddressCreationPromptAndroid = nativePlusAddressCreationPromptAndroid;
        mActivity = window.getActivity().get();
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mLayoutStateProvider = LayoutManagerProvider.from(window);
        mTabModel = tabModel;
        mTabModelSelector = tabModelSelector;
        mCoordinatorFactory = coordinatorFactory;
    }

    @VisibleForTesting
    /*package*/ static interface CoordinatorFactory {
        PlusAddressCreationCoordinator create(
                Activity activity,
                BottomSheetController bottomSheetController,
                LayoutStateProvider layoutStateProvider,
                TabModel tabModel,
                TabModelSelector tabModelSelector,
                PlusAddressCreationViewBridge bridge,
                String modalTitle,
                String plusAddressDescription,
                @Nullable String plusAddressNotice,
                String proposedPlusAddressPlaceholder,
                String plusAddressModalOkText,
                @Nullable String plusAddressModalCancelText,
                String errorReportInstruction,
                boolean refreshSupported,
                GURL learnMoreUrl,
                GURL errorReportUrl);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static PlusAddressCreationViewBridge create(
            long nativePlusAddressCreationPromptAndroid, WindowAndroid window, TabModel tabModel) {
        var tabModelSelector = TabModelSelectorSupplier.getValueOrNullFrom(window);
        assert tabModelSelector != null : "No TabModelSelector available.";
        return new PlusAddressCreationViewBridge(
                nativePlusAddressCreationPromptAndroid,
                window,
                tabModel,
                tabModelSelector,
                PlusAddressCreationCoordinator::new);
    }

    @CalledByNative
    void show(
            String modalTitle,
            String plusAddressDescription,
            @Nullable String plusAddressNotice,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            @Nullable String plusAddressModalCancelText,
            String errorReportInstruction,
            String learnMoreUrl,
            String errorReportUrl,
            boolean refreshSupported) {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            mCoordinator =
                    mCoordinatorFactory.create(
                            mActivity,
                            mBottomSheetController,
                            mLayoutStateProvider,
                            mTabModel,
                            mTabModelSelector,
                            this,
                            modalTitle,
                            plusAddressDescription,
                            plusAddressNotice,
                            proposedPlusAddressPlaceholder,
                            plusAddressModalOkText,
                            plusAddressModalCancelText,
                            errorReportInstruction,
                            refreshSupported,
                            new GURL(learnMoreUrl),
                            new GURL(errorReportUrl));
            mCoordinator.requestShowContent();
        }
    }

    @CalledByNative
    void updateProposedPlusAddress(String plusAddress) {
        if (mNativePlusAddressCreationPromptAndroid != 0 && mCoordinator != null) {
            mCoordinator.updateProposedPlusAddress(plusAddress);
        }
    }

    @CalledByNative
    void finishConfirm() {
        if (mNativePlusAddressCreationPromptAndroid != 0 && mCoordinator != null) {
            mCoordinator.finishConfirm();
        }
    }

    @CalledByNative
    void showError() {
        if (mNativePlusAddressCreationPromptAndroid != 0 && mCoordinator != null) {
            mCoordinator.showError();
        }
    }

    @CalledByNative
    void hideRefreshButton() {
        if (mNativePlusAddressCreationPromptAndroid != 0 && mCoordinator != null) {
            mCoordinator.hideRefreshButton();
        }
    }

    // Hide the bottom sheet (if showing) and clean up observers.
    @CalledByNative
    void destroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
        mNativePlusAddressCreationPromptAndroid = 0;
    }

    public void onRefreshClicked() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onRefreshClicked(
                            mNativePlusAddressCreationPromptAndroid,
                            PlusAddressCreationViewBridge.this);
        }
    }

    public void onConfirmRequested() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onConfirmRequested(
                            mNativePlusAddressCreationPromptAndroid,
                            PlusAddressCreationViewBridge.this);
        }
    }

    public void onCanceled() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onCanceled(
                            mNativePlusAddressCreationPromptAndroid,
                            PlusAddressCreationViewBridge.this);
        }
    }

    public void onPromptDismissed() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .promptDismissed(
                            mNativePlusAddressCreationPromptAndroid,
                            PlusAddressCreationViewBridge.this);
            mNativePlusAddressCreationPromptAndroid = 0;
        }
    }

    public void setActivityForTesting(Activity activity) {
        mActivity = activity;
    }

    @NativeMethods
    interface Natives {
        void onRefreshClicked(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);

        void onConfirmRequested(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);

        void onCanceled(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);

        void promptDismissed(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);
    }
}
