// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.plus_addresses;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;

/**
 * Defines Plus Address' UMA user actions. The resulting string returned by `getAction()` needs to
 * be documented at tools/metrics/actions/actions.xml.
 */
@NullMarked
public enum PlusAddressesUserActions {
    MANAGE_OPTION_ON_SETTINGS_SELECTED("ManageOptionOnSettingsSelected");

    private static final String USER_ACTION_PREFIX = "PlusAddress.";

    private final String mAction;

    PlusAddressesUserActions(final String action) {
        this.mAction = action;
    }

    String getAction() {
        return USER_ACTION_PREFIX + mAction;
    }

    public void log() {
        RecordUserAction.record(getAction());
    }
}
