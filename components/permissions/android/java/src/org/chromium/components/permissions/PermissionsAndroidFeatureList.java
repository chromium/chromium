// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.IntCachedFeatureParam;

import java.util.ArrayList;
import java.util.List;

/**
 * Lists base::Features that can be accessed through {@link PermissionsAndroidFeatureMap}.
 *
 * <p>Should be kept in sync with |kFeaturesExposedToJava| in
 * components/permissions/android/permissions_android_feature_map.cc.
 */
@NullMarked
public abstract class PermissionsAndroidFeatureList {

    public static final List<CachedFeatureParam<?>> sCachedParams = new ArrayList<>();

    public static List<CachedFeatureParam<?>> getFeatureParamsToCache() {
        return sCachedParams;
    }

    static void addCachedFeatureParam(CachedFeatureParam<?> param) {
        sCachedParams.add(param);
    }

    public static final String BLOCK_MIDI_BY_DEFAULT = "BlockMidiByDefault";

    public static final String ANDROID_CANCEL_PERMISSION_PROMPT_ON_TOUCH_OUTSIDE =
            "AndroidCancelPermissionPromptOnTouchOutside";
    public static final String PERMISSIONS_ANDROID_CLAPPER_LOUD = "PermissionsAndroidClapperLoud";
    public static final String PERMISSIONS_ANDROID_CLAPPER_QUIET = "PermissionsAndroidClapperQuiet";

    public static final String PERMISSION_ELEMENT = "PermissionElement";
    public static final String GEOLOCATION_ELEMENT = "GeolocationElement";
    public static final String BYPASS_PEPC_SECURITY_FOR_TESTING = "BypassPepcSecurityForTesting";
    public static final String PERMISSION_HEURISTIC_AUTO_GRANT = "PermissionHeuristicAutoGrant";

    public static final String APPROXIMATE_GEOLOCATION_PERMISSION =
            "ApproximateGeolocationPermission";

    public static final String AUTO_PICTURE_IN_PICTURE_ANDROID = "AutoPictureInPictureAndroid";

    public static final IntCachedFeatureParam APPROXIMATE_GEOLOCATION_PROMPT_ARM =
            PermissionsAndroidFeatureMap.newIntCachedFeatureParam(
                    APPROXIMATE_GEOLOCATION_PERMISSION,
                    "prompt_arm",
                    ApproximateGeolocationPromptArm.NO_ARM_SELECTED);
}
