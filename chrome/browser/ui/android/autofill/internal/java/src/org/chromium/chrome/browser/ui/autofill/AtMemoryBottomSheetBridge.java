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
import org.chromium.chrome.browser.profiles.Profile;
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
            BottomSheetController bottomSheetController,
            Profile profile) {
        mNativeAtMemoryBottomSheetBridge = nativeAtMemoryBottomSheetBridge;
        mCoordinator =
                new AtMemoryBottomSheetCoordinator(context, bottomSheetController, this, profile);
    }

    @CalledByNative
    public static @Nullable AtMemoryBottomSheetBridge create(
            long nativeAtMemoryBottomSheetBridge,
            WindowAndroid windowAndroid,
            @JniType("Profile*") Profile profile) {
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
                nativeAtMemoryBottomSheetBridge, context, bottomSheetController, profile);
    }

    @CalledByNative
    public void show(@JniType("std::vector") List<AutofillSuggestion> suggestions) {
        mCoordinator.show(suggestions);
    }

    @CalledByNative
    public static AutofillSuggestion createAutofillSuggestion(
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String subLabel,
            int iconId,
            int suggestionType,
            @JniType("std::vector") List<AutofillSuggestion> children) {
        return new AutofillSuggestion.Builder()
                .setLabel(label)
                .setSubLabel(subLabel)
                .setIconId(iconId)
                .setSuggestionType(suggestionType)
                .setChildren(children)
                .build();
    }

    @CalledByNative
    public void hide() {
        mCoordinator.hide();
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
    public void onQuerySubmitted(String query) {
        if (mNativeAtMemoryBottomSheetBridge != 0) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onQuerySubmitted(mNativeAtMemoryBottomSheetBridge, query);
        }
    }

    @Override
    public void onQueryTextChanged(String query) {
        if (mNativeAtMemoryBottomSheetBridge != 0) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onQueryTextChanged(mNativeAtMemoryBottomSheetBridge, query);
        }
    }

    @Override
    public void onSearchFocus(boolean hasFocus) {
        if (hasFocus) {
            mCoordinator.expandSheet();
        }
    }

    @Override
    public void onSuggestionClicked(int position) {
        if (mNativeAtMemoryBottomSheetBridge != 0) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onSuggestionSelected(mNativeAtMemoryBottomSheetBridge, position);
        }
    }

    @Override
    public void onChildSuggestionsShown(int parentPosition) {
        if (mNativeAtMemoryBottomSheetBridge != 0) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onChildSuggestionsShown(mNativeAtMemoryBottomSheetBridge, parentPosition);
        }
    }

    @Override
    public void onChildSuggestionClicked(int parentPosition, int childPosition) {
        if (mNativeAtMemoryBottomSheetBridge != 0) {
            AtMemoryBottomSheetBridgeJni.get()
                    .onChildSuggestionSelected(
                            mNativeAtMemoryBottomSheetBridge, parentPosition, childPosition);
        }
    }

    @Override
    public boolean isSearching() {
        if (mNativeAtMemoryBottomSheetBridge == 0) return false;
        return AtMemoryBottomSheetBridgeJni.get().isSearching(mNativeAtMemoryBottomSheetBridge);
    }

    @NativeMethods
    public interface Natives {
        void onDismissed(long nativeAtMemoryBottomSheetBridge);

        void onQuerySubmitted(
                long nativeAtMemoryBottomSheetBridge, @JniType("std::u16string") String query);

        void onQueryTextChanged(
                long nativeAtMemoryBottomSheetBridge, @JniType("std::u16string") String query);

        void onSuggestionSelected(long nativeAtMemoryBottomSheetBridge, int position);

        void onChildSuggestionsShown(long nativeAtMemoryBottomSheetBridge, int parentPosition);

        void onChildSuggestionSelected(
                long nativeAtMemoryBottomSheetBridge, int parentPosition, int childPosition);

        boolean isSearching(long nativeAtMemoryBottomSheetBridge);
    }
}
