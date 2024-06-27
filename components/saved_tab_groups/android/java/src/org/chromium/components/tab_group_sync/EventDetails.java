// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

/**
 * Convenient class to help passing various pieces of information to native for metrics logging
 * purposes. Most of the fields are optional, so fill them as you need. These should be used /
 * interpreted only in context of the event being recorded.
 */
public class EventDetails {
    private static final int INVALID_TAB_ID = -1;

    /** Tab group event. Mandatory. */
    public @TabGroupEvent int eventType;

    /** Tab group ID. Mandatory. */
    public LocalTabGroupId localGroupId;

    /** Tab ID. Optional. */
    public int localTabId = INVALID_TAB_ID;

    /** UI surface or action that opened a group. Required only for open event type. */
    public @OpeningSource int openingSource = OpeningSource.UNKNOWN;

    /** UI surface or action that closed a group. Required only for close event type. */
    public @ClosingSource int closingSource = ClosingSource.UNKNOWN;

    /** Constructor. */
    public EventDetails(@TabGroupEvent int eventType) {
        this.eventType = eventType;
    }
}
