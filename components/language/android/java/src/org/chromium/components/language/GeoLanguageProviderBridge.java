// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import org.jni_zero.NativeMethods;

import java.util.Arrays;
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
        return new LinkedHashSet<>(
                Arrays.asList(GeoLanguageProviderBridgeJni.get().getCurrentGeoLanguages()));
    }

    @NativeMethods
    interface Natives {
        String[] getCurrentGeoLanguages();
    }
}
