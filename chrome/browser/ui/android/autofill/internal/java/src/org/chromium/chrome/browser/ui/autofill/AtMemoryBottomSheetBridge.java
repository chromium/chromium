// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** JNI wrapper for the @memory bottom sheet. */
@NullMarked
@JNINamespace("autofill")
public class AtMemoryBottomSheetBridge implements AtMemoryBottomSheetCoordinator.Delegate {
    private long mNativeAtMemoryBottomSheetBridge;
    private final AtMemoryBottomSheetCoordinator mCoordinator;

    private AtMemoryBottomSheetBridge(
            long nativeAtMemoryBottomSheetBridge,
            Context context,
            BottomSheetController bottomSheetController) {
        mNativeAtMemoryBottomSheetBridge = nativeAtMemoryBottomSheetBridge;
        mCoordinator = new AtMemoryBottomSheetCoordinator(context, bottomSheetController, this);
    }

    @CalledByNative
    public static @Nullable AtMemoryBottomSheetBridge create(
            long nativeAtMemoryBottomSheetBridge, WindowAndroid windowAndroid) {
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return null;
        }

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            return null;
        }

        return new AtMemoryBottomSheetBridge(
                nativeAtMemoryBottomSheetBridge, context, bottomSheetController);
    }

    @CalledByNative
    public void show(@JniType("std::vector") List<AutofillSuggestion> suggestions) {
        mCoordinator.show(suggestions);
    }

    @CalledByNative
    public static AutofillSuggestion createAutofillSuggestion(
            String label, String subLabel, int iconId, int suggestionType) {
        return new AutofillSuggestion.Builder()
                .setLabel(label)
                .setSubLabel(subLabel)
                .setIconId(iconId)
                .setSuggestionType(suggestionType)
                .build();
    }

    @CalledByNative
    public void destroy() {
        mNativeAtMemoryBottomSheetBridge = 0;
        mCoordinator.hide();
    }

    @Override
    public void onDismissed() {
        if (mNativeAtMemoryBottomSheetBridge != 0) {
            AtMemoryBottomSheetBridgeJni.get().onDismissed(mNativeAtMemoryBottomSheetBridge);
        }
    }

    @Override
    public void onSuggestionClicked(AutofillSuggestion suggestion) {
        // TODO(crbug.com/513143737): Implement suggestion clicked handler
    }

    @Override
    public void onFlyoutClicked(AutofillSuggestion suggestion) {
        // TODO(crbug.com/513146609): Implement flyout clicked handler
    }

    @NativeMethods
    public interface Natives {
        void onDismissed(long nativeAtMemoryBottomSheetBridge);
    }
}
