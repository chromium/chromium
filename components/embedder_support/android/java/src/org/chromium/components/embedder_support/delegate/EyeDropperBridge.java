// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** Bridge between native EyeDropper and Android system EyeDropper intent. */
@JNINamespace("web_contents_delegate_android")
@NullMarked
public class EyeDropperBridge implements WindowAndroid.IntentCallback {
    private static final String TAG = "EyeDropperBridge";

    private long mNativeEyeDropperBridge;
    private final WindowAndroid mWindowAndroid;
    private int mRequestCode = WindowAndroid.START_INTENT_FAILURE;

    @CalledByNative
    private static @Nullable EyeDropperBridge create(
            long nativeEyeDropperBridge,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid) {
        assert windowAndroid != null;
        Context context = windowAndroid.getContext().get();
        if (ContextUtils.activityFromContext(context) == null) return null;

        Intent intent = new Intent(Intent.ACTION_OPEN_EYE_DROPPER);
        if (!windowAndroid.canResolveActivity(intent)) {
            Log.w(TAG, "No activity found to handle ACTION_OPEN_EYE_DROPPER");
            return null;
        }

        EyeDropperBridge bridge = new EyeDropperBridge(nativeEyeDropperBridge, windowAndroid);
        if (!bridge.start(intent)) {
            return null;
        }
        return bridge;
    }

    private EyeDropperBridge(long nativeEyeDropperBridge, WindowAndroid windowAndroid) {
        mNativeEyeDropperBridge = nativeEyeDropperBridge;
        mWindowAndroid = windowAndroid;
    }

    private boolean start(Intent intent) {
        mRequestCode = mWindowAndroid.showCancelableIntent(intent, this, null);
        return mRequestCode >= 0;
    }

    @CalledByNative
    private void destroy() {
        if (mRequestCode >= 0) {
            mWindowAndroid.cancelIntent(mRequestCode);
            mRequestCode = WindowAndroid.START_INTENT_FAILURE;
        }
        mNativeEyeDropperBridge = 0;
    }

    @Override
    public void onIntentCompleted(int resultCode, @Nullable Intent data) {
        mRequestCode = WindowAndroid.START_INTENT_FAILURE;
        if (mNativeEyeDropperBridge == 0) return;

        if (resultCode == Activity.RESULT_OK && data != null && data.hasExtra(Intent.EXTRA_COLOR)) {
            int color = data.getIntExtra(Intent.EXTRA_COLOR, 0);
            EyeDropperBridgeJni.get().onColorChosen(mNativeEyeDropperBridge, color);
        } else {
            EyeDropperBridgeJni.get().onColorSelectionCanceled(mNativeEyeDropperBridge);
        }
        // Native might be destroyed and `destroy()` might be called by this point.
    }

    @NativeMethods
    interface Natives {
        void onColorChosen(long nativeEyeDropperBridge, int color);

        void onColorSelectionCanceled(long nativeEyeDropperBridge);
    }
}
