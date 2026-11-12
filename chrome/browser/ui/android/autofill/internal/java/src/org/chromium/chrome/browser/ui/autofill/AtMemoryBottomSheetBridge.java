// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.Acceptability;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.AutofillSuggestion.Payload;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** JNI wrapper for the AtMemory bottom sheet. */
@NullMarked
@JNINamespace("autofill")
public class AtMemoryBottomSheetBridge implements AtMemoryBottomSheetCoordinator.Delegate {
    @JniType("::autofill::AtMemoryBottomSheetBridge")
    interface NativeAtMemoryBottomSheetBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeAtMemoryBottomSheetBridge> mNativeAtMemoryBottomSheetBridge;
    private final AtMemoryBottomSheetCoordinator mCoordinator;

    private AtMemoryBottomSheetBridge(
            JniRawPtr<NativeAtMemoryBottomSheetBridge> nativeAtMemoryBottomSheetBridge,
            Context context,
            BottomSheetController bottomSheetController,
            Profile profile) {
        mNativeAtMemoryBottomSheetBridge = nativeAtMemoryBottomSheetBridge;
        mCoordinator =
                new AtMemoryBottomSheetCoordinator(context, bottomSheetController, this, profile);
    }

    @CalledByNative
    public static @Nullable AtMemoryBottomSheetBridge create(
            JniRawPtr<NativeAtMemoryBottomSheetBridge> nativeAtMemoryBottomSheetBridge,
            WindowAndroid windowAndroid,
            @JniType("Profile*") Profile profile) {
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            nativeAtMemoryBottomSheetBridge.release();
            return null;
        }

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            nativeAtMemoryBottomSheetBridge.release();
            return null;
        }

        return new AtMemoryBottomSheetBridge(
                nativeAtMemoryBottomSheetBridge, context, bottomSheetController, profile);
    }

    @CalledByNative
    public void show(
            @JniType("std::vector") List<AutofillSuggestion> suggestions,
            @JniType("std::optional<std::u16string>") @Nullable String searchBarInitialValue) {
        mCoordinator.show(suggestions, searchBarInitialValue);
    }

    // TODO(crbug.com/534668889): Refactor isAcceptable and hasDeactivatedStyle to enum.
    @CalledByNative
    public static AutofillSuggestion createAutofillSuggestion(
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String subLabel,
            int iconId,
            int suggestionType,
            @JniType("std::vector") List<AutofillSuggestion> children,
            @Acceptability int acceptability,
            boolean hasDeactivatedStyle,
            boolean isLoading,
            @Nullable Payload payload) {
        return new AutofillSuggestion.Builder()
                .setLabel(label)
                .setSubLabel(subLabel)
                .setIconId(iconId)
                .setSuggestionType(suggestionType)
                .setChildren(children)
                .setAcceptability(acceptability)
                .setApplyDeactivatedStyle(hasDeactivatedStyle)
                .setIsLoading(isLoading)
                .setPayload(payload)
                .build();
    }

    @CalledByNative
    public void hide() {
        mCoordinator.hide();
    }

    @CalledByNative
    public void destroy() {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            mNativeAtMemoryBottomSheetBridge.release();
            mNativeAtMemoryBottomSheetBridge = null;
        }
        mCoordinator.hide();
    }

    @Override
    public void onDismissed() {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            AtMemoryBottomSheetBridgeJni.get().onDismissed(mNativeAtMemoryBottomSheetBridge);
        }
    }

    @Override
    public void onQuerySubmitted(String query) {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onQuerySubmitted(mNativeAtMemoryBottomSheetBridge, query);
        }
    }

    @Override
    public void onQueryTextChanged(String query) {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onQueryTextChanged(mNativeAtMemoryBottomSheetBridge, query);
        }
    }

    @Override
    public void requestExpandSheet(boolean expandInFullHeight) {
        mCoordinator.expand(expandInFullHeight);
    }

    @Override
    public void onSuggestionAccepted(int position) {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onSuggestionAccepted(mNativeAtMemoryBottomSheetBridge, position);
        }
    }

    @Override
    public void onSuggestionDismissed(int position) {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onSuggestionDismissed(mNativeAtMemoryBottomSheetBridge, position);
        }
    }

    @Override
    public void onChildSuggestionsShown(int parentPosition) {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onChildSuggestionsShown(mNativeAtMemoryBottomSheetBridge, parentPosition);
        }
    }

    @Override
    public void onChildSuggestionAccepted(int parentPosition, int childPosition) {
        if (mNativeAtMemoryBottomSheetBridge != null) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onChildSuggestionAccepted(
                            mNativeAtMemoryBottomSheetBridge, parentPosition, childPosition);
        }
    }

    @Override
    public boolean isSearching() {
        if (mNativeAtMemoryBottomSheetBridge == null) return false;
        return AtMemoryBottomSheetBridgeJni.get().isSearching(mNativeAtMemoryBottomSheetBridge);
    }

    @NativeMethods
    interface Natives {
        void onDismissed(JniPtr<NativeAtMemoryBottomSheetBridge> self);

        void onQuerySubmitted(
                JniPtr<NativeAtMemoryBottomSheetBridge> self,
                @JniType("std::u16string") String query);

        void onQueryTextChanged(
                JniPtr<NativeAtMemoryBottomSheetBridge> self,
                @JniType("std::u16string") String query);

        void onSuggestionAccepted(JniPtr<NativeAtMemoryBottomSheetBridge> self, int position);

        void onSuggestionDismissed(JniPtr<NativeAtMemoryBottomSheetBridge> self, int position);

        void onChildSuggestionsShown(
                JniPtr<NativeAtMemoryBottomSheetBridge> self, int parentPosition);

        void onChildSuggestionAccepted(
                JniPtr<NativeAtMemoryBottomSheetBridge> self,
                int parentPosition,
                int childPosition);

        boolean isSearching(JniPtr<NativeAtMemoryBottomSheetBridge> self);
    }
}
