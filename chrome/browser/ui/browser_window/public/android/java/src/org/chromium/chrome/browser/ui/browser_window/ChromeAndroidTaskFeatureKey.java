// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.Objects;

/** Represents a Chrome feature whose lifecycle should be in sync with {@link ChromeAndroidTask}. */
@NullMarked
public final class ChromeAndroidTaskFeatureKey {
    /** The class of the feature, used as the feature identifier. */
    public final Class<? extends ChromeAndroidTaskFeature> mFeatureClass;

    /**
     * The profile the feature is associated with, or null if the feature is not associated with a
     * profile.
     */
    public final @Nullable Profile mProfile;

    /**
     * The activity the feature is associated with, or null if the feature is not associated with an
     * activity.
     */
    public final @Nullable ActivityWindowAndroid mActivityWindowAndroid;

    /**
     * Creates a new {@link ChromeAndroidTaskFeatureKey}.
     *
     * @param featureClass The class of the feature, used as the feature identifier.
     * @param profile The profile the feature is associated with, or null if the feature is not
     *     associated with a profile.
     */
    public ChromeAndroidTaskFeatureKey(
            Class<? extends ChromeAndroidTaskFeature> featureClass, @Nullable Profile profile) {
        this(featureClass, profile, /* activityWindowAndroid= */ null);
    }

    /**
     * Creates a new {@link ChromeAndroidTaskFeatureKey}.
     *
     * @param featureClass The class of the feature, used as the feature identifier.
     * @param profile The profile the feature is associated with, or null if the feature is not
     *     associated with a profile.
     * @param activityWindowAndroid The activity the feature is associated with, or null if the
     *     feature is not associated with an activity.
     */
    public ChromeAndroidTaskFeatureKey(
            Class<? extends ChromeAndroidTaskFeature> featureClass,
            @Nullable Profile profile,
            @Nullable ActivityWindowAndroid activityWindowAndroid) {
        mFeatureClass = featureClass;
        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }
        if (o instanceof ChromeAndroidTaskFeatureKey other) {
            return mFeatureClass.equals(other.mFeatureClass)
                    && Objects.equals(mProfile, other.mProfile)
                    && Objects.equals(mActivityWindowAndroid, other.mActivityWindowAndroid);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mFeatureClass, mProfile, mActivityWindowAndroid);
    }
}
