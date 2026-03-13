// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.Objects;

/**
 * Represents a Chrome feature tracked by {@link ChromeAndroidTask}.
 *
 * <p>A feature's lifecycle is determined by this key:
 *
 * <ul>
 *   <li>If all of {@code mProfile}, {@code mActivityWindowAndroid}, and {@code mTabModel} are null,
 *       the feature is task-scoped.
 *   <li>If {@code mProfile} is not null, the feature is profile-scoped.
 *   <li>If {@code mActivityWindowAndroid} is not null, the feature is activity-scoped.
 *   <li>If {@code mTabModel} is not null, the feature is tab model-scoped. This applies only to
 *       incognito tab models on mobile which may be created and destroyed within the lifetime of
 *       the other scopes.
 *   <li>If any combination of {@code mProfile}, {@code mActivityWindowAndroid}, and {@code
 *       mTabModel} are not null, the feature is scoped to the shortest lifetime of the combination.
 * </ul>
 *
 * <p>In all cases, the feature is also bound by the lifetime of the {@link ChromeAndroidTask}.
 */
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
     * The tab model the feature is associated with, or null if the feature is not associated with a
     * tab model.
     */
    public final @Nullable TabModel mTabModel;

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
        this(featureClass, profile, activityWindowAndroid, /* tabModel= */ null);
    }

    /**
     * Creates a new {@link ChromeAndroidTaskFeatureKey}.
     *
     * @param featureClass The class of the feature, used as the feature identifier.
     * @param profile The profile the feature is associated with, or null if the feature is not
     *     associated with a profile.
     * @param activityWindowAndroid The activity the feature is associated with, or null if the
     *     feature is not associated with an activity.
     * @param tabModel The tab model the feature is associated with, or null if the feature is not
     *     associated with a tab model.
     */
    public ChromeAndroidTaskFeatureKey(
            Class<? extends ChromeAndroidTaskFeature> featureClass,
            @Nullable Profile profile,
            @Nullable ActivityWindowAndroid activityWindowAndroid,
            @Nullable TabModel tabModel) {
        mFeatureClass = featureClass;
        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;
        mTabModel = tabModel;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }
        if (o instanceof ChromeAndroidTaskFeatureKey other) {
            return mFeatureClass.equals(other.mFeatureClass)
                    && Objects.equals(mProfile, other.mProfile)
                    && Objects.equals(mActivityWindowAndroid, other.mActivityWindowAndroid)
                    && Objects.equals(mTabModel, other.mTabModel);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mFeatureClass, mProfile, mActivityWindowAndroid, mTabModel);
    }
}
