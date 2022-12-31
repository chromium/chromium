// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Intent;

/**
 * Extracted to allow testing of PermissionParamsListBuilder.
 */
public interface SystemSettingsActivityRequiredListener {
    void onSystemSettingsActivityRequired(Intent intentOverride);
}
