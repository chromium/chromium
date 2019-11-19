// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.util.LinkedHashSet;

/**
 * Bridge class that lets Android code access the native GeoLanguageProvider to
 * get the languages in the user's geographic location.
 */
public class GeoLanguageProviderBridge {
    /**
     * @return The ordered set of all languages in the user's current region, ordered by how common
     *         they are in the region.
     */
    public static LinkedHashSet<String> getCurrentGeoLanguages() {
        LinkedHashSet<String> set = new LinkedHashSet<String>();
        GeoLanguageProviderBridgeJni.get().getCurrentGeoLanguages(set);
        return set;
    }

    @CalledByNative
    private static void addGeoLanguageToSet(LinkedHashSet<String> languages, String languageCode) {
        languages.add(languageCode);
    }

    @NativeMethods
    interface Natives {
        void getCurrentGeoLanguages(LinkedHashSet<String> set);
    }
}
