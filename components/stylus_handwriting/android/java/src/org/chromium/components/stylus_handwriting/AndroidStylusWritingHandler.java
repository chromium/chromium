// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static android.view.PointerIcon.TYPE_HANDWRITING;

import android.content.ComponentName;
import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.provider.Settings;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorBoundsInfo;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/** Allows stylus handwriting using the Android stylus writing APIs introduced in Android T. */
@RequiresApi(Build.VERSION_CODES.TIRAMISU)
public class AndroidStylusWritingHandler implements StylusWritingHandler, StylusApiOption {
    private static final String TAG = "AndroidStylus";

    private final InputMethodManager mInputMethodManager;

    private StylusHandwritingInitiator mStylusHandwritingInitiator;

    public static boolean isEnabled(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return false;

        int value = -1;
        if (StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            value = StylusWritingSettingsState.getInstance().getStylusHandWritingSetting();
        } else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                value =
                        Settings.Secure.getInt(
                                context.getContentResolver(), "stylus_handwriting_enabled", 1);
            } else {
                value =
                        Settings.Global.getInt(
                                context.getContentResolver(), "stylus_handwriting_enabled", -1);
            }
        }

        if (value != 1) {
            Log.d(TAG, "Stylus feature disabled.", value);
            return false;
        }

        InputMethodManager inputMethodManager = context.getSystemService(InputMethodManager.class);
        List<InputMethodInfo> inputMethods = inputMethodManager.getInputMethodList();
        String defaultIme;
        if (StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            defaultIme = StylusWritingSettingsState.getInstance().getDefaultInputMethod();
        } else {
            defaultIme =
                    Settings.Secure.getString(
                            context.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
        }

        if (defaultIme == null) {
            Log.d(
                    TAG,
                    "Stylus handwriting feature is not supported as "
                            + "default IME could not be fetched.");
            return false;
        }

        ComponentName defaultImePackage = ComponentName.unflattenFromString(defaultIme);

        for (InputMethodInfo inputMethod : inputMethods) {
            if (!inputMethod.getComponent().equals(defaultImePackage)) continue;

            boolean result = inputMethod.supportsStylusHandwriting();

            Log.d(TAG, "Stylus feature supported by IME: %s", result);
            return result;
        }

        Log.d(TAG, "Couldn't find IME");
        return false;
    }

    AndroidStylusWritingHandler(Context context) {
        mInputMethodManager = context.getSystemService(InputMethodManager.class);
        mStylusHandwritingInitiator = new StylusHandwritingInitiator(mInputMethodManager);
    }

    @Override
    public void onWebContentsChanged(Context context, WebContents webContents) {
        webContents.setStylusWritingHandler(this);

        Log.d(TAG, "Setting on web contents, %s", webContents.getViewAndroidDelegate() != null);
        if (webContents.getViewAndroidDelegate() == null) return;

        View view = webContents.getViewAndroidDelegate().getContainerView();
        view.setAutoHandwritingEnabled(false);
    }

    @Override
    public boolean handleTouchEvent(MotionEvent event, View currentView) {
        return mStylusHandwritingInitiator.onTouchEvent(event, currentView);
    }

    @Override
    public boolean canShowSoftKeyboard() {
        return true;
    }

    @Override
    public boolean shouldInitiateStylusWriting() {
        return true;
    }

    @Override
    public EditorBoundsInfo onEditElementFocusedForStylusWriting(
            Rect focusedEditBounds,
            Point cursorPosition,
            float scaleFactor,
            int contentOffsetY,
            View view) {
        Log.d(TAG, "Start Stylus Writing");
        StylusApiOption.recordStylusHandwritingTriggered(Api.ANDROID);
        // Start stylus writing after edit element is focused so that InputConnection is current
        // focused element.
        mInputMethodManager.startStylusHandwriting(view);
        RectF bounds =
                new RectF(
                        focusedEditBounds.left / scaleFactor,
                        focusedEditBounds.top / scaleFactor,
                        focusedEditBounds.right / scaleFactor,
                        focusedEditBounds.bottom / scaleFactor);
        return new EditorBoundsInfo.Builder()
                .setEditorBounds(bounds)
                .setHandwritingBounds(bounds)
                .build();
    }

    @Override
    public EditorBoundsInfo onFocusedNodeChanged(
            Rect editableBoundsOnScreenDip,
            boolean isEditable,
            View currentView,
            float scaleFactor,
            int contentOffsetY) {
        RectF bounds = new RectF(editableBoundsOnScreenDip);
        return new EditorBoundsInfo.Builder()
                .setEditorBounds(bounds)
                .setHandwritingBounds(bounds)
                .build();
    }

    @Override
    public int getStylusPointerIcon() {
        return TYPE_HANDWRITING;
    }

    void setHandwritingInitiatorForTesting(StylusHandwritingInitiator stylusHandwritingInitiator) {
        mStylusHandwritingInitiator = stylusHandwritingInitiator;
    }
}
