// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
@IntDef({
    ApproximateGeolocationPromptArm.NO_ARM_SELECTED,
    ApproximateGeolocationPromptArm.ARM_1,
    ApproximateGeolocationPromptArm.ARM_2,
    ApproximateGeolocationPromptArm.ARM_3,
    ApproximateGeolocationPromptArm.ARM_4,
    ApproximateGeolocationPromptArm.ARM_5,
    ApproximateGeolocationPromptArm.ARM_6
})
@Retention(RetentionPolicy.SOURCE)
public @interface ApproximateGeolocationPromptArm {
    int NO_ARM_SELECTED = 0;
    int ARM_1 = 1;
    int ARM_2 = 2;
    int ARM_3 = 3;
    int ARM_4 = 4;
    int ARM_5 = 5;
    int ARM_6 = 6;
}
