// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFlag;

import java.util.List;

/**
 * Lists base::Features that can be accessed through {@link MediaFeatureMap}.
 *
 * <p>Uses {@link org.chromium.base.FeatureMap} to enable {@link
 * org.chromium.base.cached_flags.CachedFlag}. This is necessary because some media events (like
 * screen off/on broadcasts) can occur early in the application lifecycle before the native library
 * is fully initialized. {@link org.chromium.base.cached_flags.CachedFlag} provides a safe way to
 * check feature state using SharedPreferences.
 *
 * <p>Should be kept in sync with |kFeaturesExposedToJava| in
 * //components/browser_ui/media/android/media_feature_map.cc
 */
@NullMarked
public abstract class MediaFeatureList {
    public static final String PAUSE_MEDIA_ON_SYSTEM_SLEEP_ANDROID =
            "PauseMediaOnSystemSleepAndroid";

    public static final CachedFlag sPauseMediaOnSystemSleepAndroid =
            new CachedFlag(
                    MediaFeatureMap.getInstance(),
                    PAUSE_MEDIA_ON_SYSTEM_SLEEP_ANDROID,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final List<CachedFlag> sAllCachedFlags = List.of(sPauseMediaOnSystemSleepAndroid);

    /** Returns all cached flags for the media component. */
    public static List<CachedFlag> getAllCachedFlags() {
        return sAllCachedFlags;
    }
}
