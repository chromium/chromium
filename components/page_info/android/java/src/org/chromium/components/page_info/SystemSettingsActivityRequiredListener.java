// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Intent;

import org.chromium.build.annotations.NullMarked;

/** Extracted to allow testing of PermissionParamsListBuilder. */
@NullMarked
public interface SystemSettingsActivityRequiredListener {
    void onSystemSettingsActivityRequired(Intent intentOverride);
}
