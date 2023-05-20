// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import org.chromium.base.FeatureMap;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for translate base::Features.
 *
 * Note: Features must be added to the array |kFeaturesExposedToJava| in
 * //components/translate/content/android/translate_feature_map.cc
 */
@JNINamespace("translate::android")
public final class TranslateFeatureMap extends FeatureMap {
    /** Alphabetical: */
    public static final String CONTENT_LANGUAGES_DISABLE_OBSERVERS_PARAM = "disable_observers";
    public static final String CONTENT_LANGUAGES_IN_LANGUAGE_PICKER =
            "ContentLanguagesInLanguagePicker";
    private static final TranslateFeatureMap sInstance = new TranslateFeatureMap();

    // Do not instantiate this class.
    private TranslateFeatureMap() {}

    /**
     * @return the singleton TranslateFeatureMap.
     */
    public static TranslateFeatureMap getInstance() {
        return sInstance;
    }

    @Override
    protected long getNativeMap() {
        return TranslateFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
