// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.plus_addresses;

import org.chromium.base.metrics.RecordHistogram;

/** This class provides helpers to record metrics related to the plus addresses. */
public class PlusAddressesMetricsRecorder {
    public static final String UMA_PLUS_ADDRESSES_OPEN_ACCOUNT_SETTINGS =
            "PlusAddresses.AccountSettings.Launch.Success";

    public static void recordAccountSettingsLaunched(boolean success) {
        RecordHistogram.recordBooleanHistogram(UMA_PLUS_ADDRESSES_OPEN_ACCOUNT_SETTINGS, success);
    }
}
