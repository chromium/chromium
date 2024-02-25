// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

/**
 * Java representation of the TriggerDetails object. Used to determine whether or not to trigger
 * any help UI with snooze button.
 */
public class TriggerDetails {
    /**
     * Construct an instance of TriggerDetails.
     * @param shouldTriggerIph Whether or not in-product help should be shown.
     * @param shouldShowSnooze Whether or not snooze button should be shown.
     */
    public TriggerDetails(boolean shouldTriggerIph, boolean shouldShowSnooze) {
        this.shouldTriggerIph = shouldTriggerIph;
        this.shouldShowSnooze = shouldShowSnooze;
    }

    /** Whether or not in-product help should be shown. */
    public final boolean shouldTriggerIph;

    /** Whether or not snooze button should be shown. Only valid if shouldTriggerIph is true. */
    public final boolean shouldShowSnooze;
}
