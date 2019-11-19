// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * ColorChooserAndroid communicates with the java ColorPickerDialog and the
 * native color_chooser_android.cc
 */
@JNINamespace("web_contents_delegate_android")
public class ColorChooserAndroid {
    private final ColorPickerDialog mDialog;
    private final long mNativeColorChooserAndroid;

    private ColorChooserAndroid(long nativeColorChooserAndroid, Context context, int initialColor,
            ColorSuggestion[] suggestions) {
        OnColorChangedListener listener = new OnColorChangedListener() {
            @Override
            public void onColorChanged(int color) {
                mDialog.dismiss();
                ColorChooserAndroidJni.get().onColorChosen(
                        mNativeColorChooserAndroid, ColorChooserAndroid.this, color);
            }
        };

        mNativeColorChooserAndroid = nativeColorChooserAndroid;
        mDialog = new ColorPickerDialog(context, listener, initialColor, suggestions);
    }

    private void openColorChooser() {
        mDialog.show();
    }

    @CalledByNative
    public void closeColorChooser() {
        mDialog.dismiss();
    }

    @CalledByNative
    public static ColorChooserAndroid createColorChooserAndroid(long nativeColorChooserAndroid,
            WindowAndroid windowAndroid, int initialColor, ColorSuggestion[] suggestions) {
        if (windowAndroid == null) return null;
        Context windowContext = windowAndroid.getContext().get();
        if (ContextUtils.activityFromContext(windowContext) == null) return null;
        ColorChooserAndroid chooser = new ColorChooserAndroid(
                nativeColorChooserAndroid, windowContext, initialColor, suggestions);
        chooser.openColorChooser();
        return chooser;
    }

    @CalledByNative
    private static ColorSuggestion[] createColorSuggestionArray(int size) {
        return new ColorSuggestion[size];
    }

    /**
     * @param array ColorSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param color Color of the suggestion.
     * @param label Label of the suggestion.
     */
    @CalledByNative
    private static void addToColorSuggestionArray(
            ColorSuggestion[] array, int index, int color, String label) {
        array[index] = new ColorSuggestion(color, label);
    }

    @NativeMethods
    interface Natives {
        // Implemented in color_chooser_android.cc
        void onColorChosen(long nativeColorChooserAndroid, ColorChooserAndroid caller, int color);
    }
}
