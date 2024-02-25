// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

/** Contains all of the command line switches for external intent launching. */
public abstract class ExternalIntentsSwitches {
    /** Never forward URL requests to external intents. */
    public static final String DISABLE_EXTERNAL_INTENT_REQUESTS =
            "disable-external-intent-requests";

    // Prevent instantiation.
    private ExternalIntentsSwitches() {}
}
