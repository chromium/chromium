// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class enables the communication between the Java and Native sides of the code. The basic
 * flow is: Methods create() and showColorPicker() get called when the Color Picker is opened. When
 * the user is done, pressing "Set", "Cancel" or anywhere other than the Color Picker view, will
 * trigger onDialogDismissed() and closeColorPicker() methods.
 */
@JNINamespace("web_contents_delegate_android")
@NullMarked
public class ColorPickerBridge {
    private final long mNativeDialog;
    private final ColorPickerCoordinator mColorPickerCoordinator;

    @CalledByNative
    static @Nullable ColorPickerBridge create(long nativeDialog, WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (ContextUtils.activityFromContext(context) == null) return null;
        assumeNonNull(context);
        return new ColorPickerBridge(nativeDialog, context);
    }

    private ColorPickerBridge(long nativeDialog, Context context) {
        mNativeDialog = nativeDialog;
        mColorPickerCoordinator = ColorPickerCoordinator.create(context, this::onDialogDismissed);
    }

    @CalledByNative
    void showColorPicker(int initialColor) {
        mColorPickerCoordinator.show(initialColor);
    }

    @CalledByNative
    void closeColorPicker() {
        mColorPickerCoordinator.close();
    }

    @CalledByNative
    void addColorSuggestion(int color, String label) {
        if (mColorPickerCoordinator != null) {
            mColorPickerCoordinator.addColorSuggestion(color, label);
        }
    }

    void onDialogDismissed(int newColor) {
        ColorPickerBridgeJni.get().onColorChosen(mNativeDialog, newColor);
    }

    @NativeMethods
    interface Natives {
        // Implemented in color_picker_bridge.cc
        void onColorChosen(long nativeColorPickerBridge, int color);
    }
}
