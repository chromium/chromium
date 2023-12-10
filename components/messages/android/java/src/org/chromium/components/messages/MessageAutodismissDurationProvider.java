// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/** Interface to provide a duration time for message. */
public interface MessageAutodismissDurationProvider {
    /**
     * Provide a duration time based on given custom duration and whether a11y mode is on.
     * @param messageIdentifier Unique {@link MessageIdentifier} of the message.
     * @param customDuration customDuration in milliseconds in non-a11y mode.
     *        Set 0 to get default duration.
     * @return The expected duration for the message.
     */
    long get(@MessageIdentifier int messageIdentifier, long customDuration);
}
