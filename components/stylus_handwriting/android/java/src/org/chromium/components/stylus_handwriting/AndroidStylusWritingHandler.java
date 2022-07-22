// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.provider.Settings;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.WebContents;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Allows stylus handwriting using the Android stylus writing APIs introduced in Android T.
 */
// TODO(peconn): Comment out once we have that build code.
// @RequiresApi(Build.VERSION_CODES.T)
public class AndroidStylusWritingHandler implements StylusWritingHandler, StylusApiOption {
    private static final String TAG = "AndroidStylus";

    private final InputMethodManager mInputMethodManager;
    private View mTargetView;

    public static boolean isEnabled(Context context) {
        if (!BuildInfo.isAtLeastT()) return false;

        int value = Settings.Global.getInt(
                context.getContentResolver(), "stylus_handwriting_enabled", -1);

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

            // We can't create a boolean here and set it in a lambda, so use AtomicBoolean instead.
            AtomicBoolean result = new AtomicBoolean();

            // The following reflection executes:
            // inputMethod.supportsStylusHandwriting();
            doReflection(() -> {
                Method supportsStylusHandwriting =
                        inputMethod.getClass().getMethod("supportsStylusHandwriting");
                Boolean supports = (Boolean) supportsStylusHandwriting.invoke(inputMethod);
                result.set(supports.booleanValue());
            });

            Log.d(TAG, "Stylus feature supported by IME: %s", result.get());
            return result.get();
        }

        Log.d(TAG, "Couldn't find IME");
        return false;
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

        // The following reflection executes:
        // view.setAutoHandwritingEnabled(false);
        doReflection(() -> {
            Method setAutoHandwritingEnabled =
                    view.getClass().getMethod("setAutoHandwritingEnabled", boolean.class);
            setAutoHandwritingEnabled.invoke(view, false);
        });

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
        // The following reflection executes:
        // mInputMethodManager.startStylusHandwriting(mTargetView);
        doReflection(() -> {
            Method startStylusHandwriting =
                    mInputMethodManager.getClass().getMethod("startStylusHandwriting", View.class);
            startStylusHandwriting.invoke(mInputMethodManager, mTargetView);
        });

        return true;
    }

    @Override
    public void onEditElementFocusedForStylusWriting(Rect focusedEditBounds, Point cursorPosition) {
        CursorAnchorInfo.Builder cursorAnchorInfoBuilder = new CursorAnchorInfo.Builder();
        RectF bounds = new RectF(focusedEditBounds);

        // The following reflection executes:
        // EditorBoundsInfo editorBoundsInfo = new EditorBoundsInfo.Builder()
        //         .setHandwritingBounds(bounds)
        //         .build();
        // cursorAnchorInfoBuilder.setEditorBoundsInfo(editorBoundsInfo);
        doReflection(() -> {
            // EditorBoundsInfo.Builder editorBoundsInfoBuilder = new EditorBoundsInfo.Builder();
            Class<?> editorBoundsInfoBuilderClass =
                    Class.forName("android.view.inputmethod.EditorBoundsInfo$Builder");
            Object editorBoundsInfoBuilder =
                    editorBoundsInfoBuilderClass.getConstructor().newInstance();

            // editorBoundsInfoBuilder.setHandwritingBounds(bounds)
            Method setHandwritingBounds =
                    editorBoundsInfoBuilderClass.getMethod("setHandwritingBounds", RectF.class);
            setHandwritingBounds.invoke(editorBoundsInfoBuilder, bounds);

            // EditorBoundsInfo editorBoundsInfo = editorBoundsInfoBuilder.build();
            Method build = editorBoundsInfoBuilderClass.getMethod("build");
            Object editorBoundsInfo = build.invoke(editorBoundsInfoBuilder);

            // cursorAnchorInfoBuilder.setEditorBoundsInfo(editorBoundsInfo);
            Class<?> editorBoundsInfoClass =
                    Class.forName("android.view.inputmethod.EditorBoundsInfo");
            Method setEditorBoundsInfo = cursorAnchorInfoBuilder.getClass().getMethod(
                    "setEditorBoundsInfo", editorBoundsInfoClass);
            setEditorBoundsInfo.invoke(cursorAnchorInfoBuilder, editorBoundsInfo);
        });

        mInputMethodManager.updateCursorAnchorInfo(mTargetView, cursorAnchorInfoBuilder.build());
    }

    @FunctionalInterface
    private interface ReflectionCallback {
        void run() throws ClassNotFoundException, IllegalAccessException, InvocationTargetException,
                          NoSuchMethodException, InstantiationException;
    }

    private static void doReflection(ReflectionCallback callback) {
        // We aren't building against the Android T SDK yet, so we need to use reflection to use
        // methods/classes introduced in Android T.
        try {
            callback.run();
        } catch (ClassNotFoundException | IllegalAccessException | InstantiationException
                | InvocationTargetException | NoSuchMethodException e) {
            // This method should *only* be called if isEnabled returns true, and that already
            // checks that we're on Android T, so if an exception happens here, there's a bug
            // somewhere.
            throw new RuntimeException("Reflection failed in AndroidStylusWritingHandler", e);
        }
    }
}
