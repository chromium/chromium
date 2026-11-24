// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.graphics.Bitmap;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Exposes the native omnibox::OmniboxIconSource, which renders icons identified by their
 * IconResourceIds value, e.g. server-delivered AIM tool and model icons. An instance forwards icon
 * set changes to its owner until {@link #destroy()} is called. All methods must be called on the UI
 * thread after native initialization.
 */
@JNINamespace("omnibox")
@NullMarked
public class OmniboxIconBridge {
    /** The native counterpart that observes the icon source on behalf of this bridge. */
    @JniType("omnibox::OmniboxIconBridge")
    interface NativeOmniboxIconBridge extends JniTypeToken {}

    /** An icon rendered by the native source. */
    public static final class RenderedIcon {
        /** The square icon bitmap. Monochrome icons are black and should be tinted. */
        public final Bitmap bitmap;

        /** Whether the icon keeps its own colors and must not be tinted. */
        public final boolean isMultiColor;

        @CalledByNative
        @VisibleForTesting
        public RenderedIcon(@JniType("SkBitmap") Bitmap bitmap, boolean isMultiColor) {
            this.bitmap = bitmap;
            this.isMultiColor = isMultiColor;
        }
    }

    private final Runnable mOnIconsChanged;
    private final JniUniquePtr<NativeOmniboxIconBridge> mNativeBridge;

    /**
     * Starts forwarding icon set changes to {@code onIconsChanged} until {@link #destroy()} is
     * called.
     *
     * @param onIconsChanged Run when the set of available icons changes, so icons previously
     *     returned by {@link #getIcon} may be outdated.
     */
    public OmniboxIconBridge(Runnable onIconsChanged) {
        ThreadUtils.assertOnUiThread();
        mOnIconsChanged = onIconsChanged;
        mNativeBridge = OmniboxIconBridgeJni.get().init(this);
    }

    /**
     * Returns icon {@code iconId} (an IconResourceIds value) rendered as a {@code sizePx} x {@code
     * sizePx} bitmap, together with its color mode, or null if the icon is unknown. Ids newer than
     * this client's IconResourceIds are allowed, since the icon pack may carry them. Rendered icons
     * may become outdated; the owner is notified through the callback passed to the constructor.
     *
     * @throws IllegalStateException if called after {@link #destroy()}.
     */
    public @Nullable RenderedIcon getIcon(int iconId, @Px int sizePx) {
        ThreadUtils.assertOnUiThread();
        return OmniboxIconBridgeJni.get().getIcon(mNativeBridge, iconId, sizePx);
    }

    /**
     * Stops forwarding icon set changes and frees the native counterpart. {@link #getIcon} must not
     * be called afterwards.
     */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.destroy();
    }

    /** Notifies the owner that the native icon set changed. */
    @CalledByNative
    @VisibleForTesting
    void onIconsChanged() {
        ThreadUtils.assertOnUiThread();
        mOnIconsChanged.run();
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeOmniboxIconBridge> init(OmniboxIconBridge bridge);

        @Nullable RenderedIcon getIcon(
                JniPtr<NativeOmniboxIconBridge> self,
                @JniType("omnibox::IconResourceIds") int iconId,
                @Px int sizePx);
    }
}
