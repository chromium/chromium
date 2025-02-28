// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import org.chromium.build.annotations.NullMarked;

/**
 * Lists base::Features that can be accessed through {@link PermissionsAndroidFeatureMap}.
 *
 * Should be kept in sync with |kFeaturesExposedToJava| in
 * components/permissions/android/permissions_android_feature_map.cc.
 */
@NullMarked
public abstract class PermissionsAndroidFeatureList {
    public static final String BLOCK_MIDI_BY_DEFAULT = "BlockMidiByDefault";

    public static final String ONE_TIME_PERMISSION = "OneTimePermission";

    public static final String ANDROID_CANCEL_PERMISSION_PROMPT_ON_TOUCH_OUTSIDE =
            "AndroidCancelPermissionPromptOnTouchOutside";

    public static final String PERMISSION_ELEMENT = "PermissionElement";
    public static final String BYPASS_PEPC_SECURITY_FOR_TESTING = "BypassPepcSecurityForTesting";

    public static final String OS_ADDITIONAL_SECURITY_PERMISSION_KILL_SWITCH =
            "OsAdditionalSecurityPermissionKillSwitch";
}
