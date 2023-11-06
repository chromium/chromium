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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for C++ PlusAddressCreationViewAndroid. */
@JNINamespace("plus_addresses")
public class PlusAddressCreationViewBridge {
    private long mNativePlusAddressCreationPromptAndroid;
    private Activity mActivity;
    private BottomSheetController mBottomSheetController;
    private LayoutStateProvider mLayoutStateProvider;
    private final TabModel mTabModel;
    private CoordinatorFactory mCoordinatorFactory;
    @Nullable private PlusAddressCreationCoordinator mCoordinator;

    @VisibleForTesting
    /*package*/ PlusAddressCreationViewBridge(
            long nativePlusAddressCreationPromptAndroid,
            WindowAndroid window,
            TabModel tabModel,
            CoordinatorFactory coordinatorFactory) {
        mNativePlusAddressCreationPromptAndroid = nativePlusAddressCreationPromptAndroid;
        mActivity = window.getActivity().get();
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mLayoutStateProvider = LayoutManagerProvider.from(window);
        mTabModel = tabModel;
        mCoordinatorFactory = coordinatorFactory;
    }

    @VisibleForTesting
    /*package*/ static interface CoordinatorFactory {
        PlusAddressCreationCoordinator create(
                Activity activity,
                BottomSheetController bottomSheetController,
                LayoutStateProvider layoutStateProvider,
                TabModel tabModel,
                PlusAddressCreationViewBridge bridge,
                String modalTitle,
                String plusAddressDescription,
                String proposedPlusAddressPlaceholder,
                String plusAddressModalOkText,
                String plusAddressModalCancelText);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static PlusAddressCreationViewBridge create(
            long nativePlusAddressCreationPromptAndroid, WindowAndroid window, TabModel tabModel) {
        return new PlusAddressCreationViewBridge(
                nativePlusAddressCreationPromptAndroid,
                window,
                tabModel,
                PlusAddressCreationCoordinator::new);
    }

    @CalledByNative
    void show(
            String modalTitle,
            String plusAddressDescription,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            String plusAddressModalCancelText) {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            mCoordinator =
                    mCoordinatorFactory.create(
                            mActivity,
                            mBottomSheetController,
                            mLayoutStateProvider,
                            mTabModel,
                            this,
                            modalTitle,
                            plusAddressDescription,
                            proposedPlusAddressPlaceholder,
                            plusAddressModalOkText,
                            plusAddressModalCancelText);
            mCoordinator.requestShowContent();
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

    public void onConfirmed() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onConfirmed(
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
        void onConfirmed(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);
        void onCanceled(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);
        void promptDismissed(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);
    }
}
