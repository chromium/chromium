// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutManagerProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for C++ PlusAddressCreationViewAndroid. */
@NullMarked
@JNINamespace("plus_addresses")
public class PlusAddressCreationViewBridge {
    private long mNativePlusAddressCreationPromptAndroid;
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final LayoutStateProvider mLayoutStateProvider;
    private final TabModel mTabModel;
    private final TabModelSelector mTabModelSelector;
    private final CoordinatorFactory mCoordinatorFactory;
    @Nullable private PlusAddressCreationCoordinator mCoordinator;

    @VisibleForTesting
    /*package*/ PlusAddressCreationViewBridge(
            long nativePlusAddressCreationPromptAndroid,
            Context context,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            TabModelSelector tabModelSelector,
            CoordinatorFactory coordinatorFactory) {
        mNativePlusAddressCreationPromptAndroid = nativePlusAddressCreationPromptAndroid;
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mLayoutStateProvider = layoutStateProvider;
        mTabModel = tabModel;
        mTabModelSelector = tabModelSelector;
        mCoordinatorFactory = coordinatorFactory;
    }

    @VisibleForTesting
    /*package*/ interface CoordinatorFactory {
        PlusAddressCreationCoordinator create(
                Context context,
                BottomSheetController bottomSheetController,
                LayoutStateProvider layoutStateProvider,
                TabModel tabModel,
                TabModelSelector tabModelSelector,
                PlusAddressCreationViewBridge bridge,
                PlusAddressCreationNormalStateInfo info,
                boolean refreshSupported);
    }

    @CalledByNative
    @VisibleForTesting
    @Nullable
    static PlusAddressCreationViewBridge create(
            long nativePlusAddressCreationPromptAndroid, WindowAndroid window, TabModel tabModel) {
        var tabModelSelector = TabModelSelectorSupplier.getValueOrNullFrom(window);
        if (tabModelSelector == null || window.getActivity().get() == null) {
            return null;
        }
        return new PlusAddressCreationViewBridge(
                nativePlusAddressCreationPromptAndroid,
                window.getActivity().get(),
                assumeNonNull(BottomSheetControllerProvider.from(window)),
                assumeNonNull(LayoutManagerProvider.from(window)),
                tabModel,
                tabModelSelector,
                PlusAddressCreationCoordinator::new);
    }

    @CalledByNative
    void show(PlusAddressCreationNormalStateInfo info, boolean refreshSupported) {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            mCoordinator =
                    mCoordinatorFactory.create(
                            mContext,
                            mBottomSheetController,
                            mLayoutStateProvider,
                            mTabModel,
                            mTabModelSelector,
                            this,
                            info,
                            refreshSupported);
            mCoordinator.requestShowContent();
        }
    }

    @CalledByNative
    void updateProposedPlusAddress(@JniType("std::string") String plusAddress) {
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

    /**
     * Shows the error screen specified by {@code errorStateInfo}.
     *
     * @param errorStateInfo necassary UI information to show a meaningful error message to the
     *     user.
     */
    @CalledByNative
    void showError(PlusAddressCreationErrorStateInfo errorStateInfo) {
        if (mNativePlusAddressCreationPromptAndroid != 0 && mCoordinator != null) {
            mCoordinator.showError(errorStateInfo);
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
                    .onRefreshClicked(mNativePlusAddressCreationPromptAndroid);
        }
    }

    public void tryAgainToReservePlusAddress() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .tryAgainToReservePlusAddress(mNativePlusAddressCreationPromptAndroid);
        }
    }

    public void onConfirmRequested() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onConfirmRequested(mNativePlusAddressCreationPromptAndroid);
        }
    }

    public void onCanceled() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onCanceled(mNativePlusAddressCreationPromptAndroid);
        }
    }

    public void onPromptDismissed() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .promptDismissed(mNativePlusAddressCreationPromptAndroid);
            mNativePlusAddressCreationPromptAndroid = 0;
        }
    }

    @NativeMethods
    interface Natives {
        void onRefreshClicked(long nativePlusAddressCreationViewAndroid);

        void tryAgainToReservePlusAddress(long nativePlusAddressCreationViewAndroid);

        void onConfirmRequested(long nativePlusAddressCreationViewAndroid);

        void onCanceled(long nativePlusAddressCreationViewAndroid);

        void promptDismissed(long nativePlusAddressCreationViewAndroid);
    }
}
