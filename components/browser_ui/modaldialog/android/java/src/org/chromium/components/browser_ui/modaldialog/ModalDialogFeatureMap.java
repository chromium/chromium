// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.components.cached_flags.CachedFlag;

import java.util.List;

/** Java accessor for base::Features listed in {@link ModalDialogFeatureList}. */
@JNINamespace("browser_ui")
public final class ModalDialogFeatureMap extends FeatureMap {
    private static final ModalDialogFeatureMap sInstance = new ModalDialogFeatureMap();

    public static final CachedFlag sModalDialogLayoutWithSystemInsets =
            new CachedFlag(
                    sInstance, ModalDialogFeatureList.MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS, true);
    public static final List<CachedFlag> sCachedFlags = List.of(sModalDialogLayoutWithSystemInsets);

    // Do not instantiate this class.
    private ModalDialogFeatureMap() {}

    /**
     * @return the singleton ModalDialogFeatureMap.
     */
    public static ModalDialogFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return ModalDialogFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {

        long getNativeMap();
    }
}
