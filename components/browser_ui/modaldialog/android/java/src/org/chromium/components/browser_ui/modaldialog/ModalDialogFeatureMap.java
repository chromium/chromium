// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import android.content.Context;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/** Java accessor for base::Features listed in {@link ModalDialogFeatureList}. */
@JNINamespace("browser_ui")
@NullMarked
public final class ModalDialogFeatureMap extends FeatureMap {
    private static final ModalDialogFeatureMap sInstance = new ModalDialogFeatureMap();

    public static final CachedFlag sDialogsOnLargeFormFactors =
            new CachedFlag(sInstance, ModalDialogFeatureList.DIALOGS_ON_LARGE_FORM_FACTORS, false);
    public static final CachedFlag sModalDialogLayoutWithSystemInsets =
            new CachedFlag(
                    sInstance, ModalDialogFeatureList.MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS, true);
    public static final List<CachedFlag> sCachedFlags =
            List.of(sDialogsOnLargeFormFactors, sModalDialogLayoutWithSystemInsets);

    /**
     * Returns whether large form factor modal dialog UI updates should be applied.
     *
     * @param context The {@link Context} associated with the window or activity.
     * @return True if the feature flag is enabled and the form factor is a large form factor.
     */
    public static boolean isLargeFormFactorUiEnabled(Context context) {
        return sDialogsOnLargeFormFactors.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

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
