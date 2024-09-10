// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;

/** Java accessor for base::Features listed in {@link ModalDialogFeatureList}. */
@JNINamespace("browser_ui")
public final class ModalDialogFeatureMap extends FeatureMap {
    private static final ModalDialogFeatureMap sInstance = new ModalDialogFeatureMap();
    private static boolean sModalDialogLayoutWithSystemInsetsEnabledForTesting;

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
        if (BuildConfig.IS_FOR_TEST
                && ModalDialogFeatureList.MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS.equals(
                        featureName)) {
            // Disable this feature in all tests by default, unless enabled explicitly.
            // TODO (crbug/339304231): Use @DisableFeatures/@EnableFeatures in affected test files.
            return sModalDialogLayoutWithSystemInsetsEnabledForTesting;
        }
        return getInstance().isEnabledInNative(featureName);
    }

    /**
     * @param enabled Whether MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS should be enabled for testing.
     */
    public static void setModalDialogLayoutWithSystemInsetsEnabledForTesting(boolean enabled) {
        sModalDialogLayoutWithSystemInsetsEnabledForTesting = enabled;
        ResettersForTesting.register(
                () -> sModalDialogLayoutWithSystemInsetsEnabledForTesting = false);
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
