// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

/** Implements a CronetLogger that does nothing. */
public final class NoOpLogger extends CronetLogger {
    @Override
    public void logCronetEngineCreation(
            int cronetEngineId,
            CronetEngineBuilderInfo engineBuilderInfo,
            CronetVersion version,
            CronetSource source) {}

    @Override
    public void logCronetTrafficInfo(int cronetEngineId, CronetTrafficInfo trafficInfo) {}
}
