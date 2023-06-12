// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;
import android.os.Build;
import android.provider.Settings;
import android.view.PointerIcon;
import android.view.View;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.annotation.RequiresApi;
import androidx.core.os.BuildCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate.StylusWritingCursorHandler;

import java.lang.reflect.Field;
import java.util.List;

/**
 * Allows stylus handwriting using the Android stylus writing APIs introduced in Android T.
 */
@RequiresApi(Build.VERSION_CODES.TIRAMISU)
public class AndroidStylusWritingHandler
        implements StylusWritingHandler, StylusApiOption, StylusWritingCursorHandler {
    private static final String TAG = "AndroidStylus";
    private static @Nullable Integer sHandwritingPointerType = getHandwritingHoverPointer();

    private final InputMethodManager mInputMethodManager;
    private View mTargetView;

    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    public static boolean isEnabled(Context context) {
        if (!BuildInfo.isAtLeastT()) return false;

        int value = -1;
        if (BuildCompat.isAtLeastU()) {
            value = Settings.Secure.getInt(
                    context.getContentResolver(), "stylus_handwriting_enabled", 1);
        } else {
            value = Settings.Global.getInt(
                    context.getContentResolver(), "stylus_handwriting_enabled", -1);
        }

        if (value != 1) {
            Log.d(TAG, "Stylus feature disabled.", value);
            return false;
        }

        InputMethodManager inputMethodManager = context.getSystemService(InputMethodManager.class);
        List<InputMethodInfo> inputMethods = inputMethodManager.getInputMethodList();
        String defaultImePackage = Settings.Secure.getString(
                context.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);

        for (InputMethodInfo inputMethod : inputMethods) {
            if (!inputMethod.getComponent().flattenToString().equals(defaultImePackage)) continue;

            boolean result = inputMethod.supportsStylusHandwriting();

            Log.d(TAG, "Stylus feature supported by IME: %s", result);
            return result;
        }

        Log.d(TAG, "Couldn't find IME");
        return false;
    }

    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    private static @Nullable Integer getHandwritingHoverPointer() {
        Integer handwritingPointerType = null;
        // Android handwriting hover icon is supported from Android U.
        if (BuildCompat.isAtLeastU()) {
            // TODO(crbug.com/1416170): Remove this reflection code when Android U is released.
            try {
                Field handwritingPointer = PointerIcon.class.getField("TYPE_HANDWRITING");
                handwritingPointerType = (Integer) handwritingPointer.get(null);
            } catch (NoSuchFieldException | IllegalAccessException e) {
                // Do nothing.
            }
        }
        return handwritingPointerType;
    }

    AndroidStylusWritingHandler(Context context) {
        mInputMethodManager = context.getSystemService(InputMethodManager.class);
    }

    @Override
    public void onWebContentsChanged(Context context, WebContents webContents) {
        webContents.setStylusWritingHandler(this);

        Log.d(TAG, "Setting on web contents, %s", webContents.getViewAndroidDelegate() != null);
        if (webContents.getViewAndroidDelegate() == null) return;

        View view = webContents.getViewAndroidDelegate().getContainerView();
        view.setAutoHandwritingEnabled(false);

        mTargetView = view;
    }

    @Override
    public void onWindowFocusChanged(Context context, boolean hasFocus) {}

    @Override
    public boolean canShowSoftKeyboard() {
        // TODO(mahesh.ma): We can return false here when Android stylus writing service has widget
        // toolbar that can allow editing commands like add space, backspace, perform editor actions
        // like next, prev, search, go etc, or an option to show/hide keyboard. Until then it is
        // better to allow showing soft keyboard for above operations. It can be noted that Platform
        // Edit text behaviour is also to show soft keyboard during stylus writing in Android T.
        return true;
    }

    @Override
    public boolean requestStartStylusWriting(StylusWritingImeCallback imeCallback) {
        Log.d(TAG, "Requesting Stylus Writing");
        StylusApiOption.recordStylusHandwritingTriggered(Api.ANDROID);
        mInputMethodManager.startStylusHandwriting(mTargetView);
        return true;
    }

    @Override
    public StylusWritingCursorHandler getStylusWritingCursorHandler() {
        return this;
    }

    @Override
    public boolean didHandleCursorUpdate(View currentView) {
        if (sHandwritingPointerType == null) return false;
        // Enable this icon behind feature flag that shows hover Icon in expanded area of target.
        if (!ContentFeatureMap.isEnabled(BlinkFeatures.STYLUS_POINTER_ADJUSTMENT)) return false;

        currentView.setPointerIcon(
                PointerIcon.getSystemIcon(currentView.getContext(), sHandwritingPointerType));
        return true;
    }
}
