// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents a message metadata for an aggregated instant message. Contains the list of
 * attributions for the individual messages that it represents. See
 * //components/collaboration/public/messaging/message.h for specific details.
 */
@NullMarked
public class AggregatedMessageData {
    // List of message attributions that is being aggregated in the instant message.
    public List<MessageAttribution> attributions = new ArrayList<>();
}
