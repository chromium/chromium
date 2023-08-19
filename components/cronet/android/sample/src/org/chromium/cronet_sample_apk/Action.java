// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import org.chromium.net.ConnectionMigrationOptions;
import org.chromium.net.CronetEngine;
import org.chromium.net.QuicOptions;

/**
 * Actions provide a way for an Option to configure CronetEngine builder according to its specific
 * logic which makes {@link Options} not tightly coupled to {@link CronetEngine.Builder}, {@link
 * ConnectionMigrationOptions.Builder} and {@link QuicOptions.Builder}
 *
 * <p> Actions are applied {@link CronetSampleApplication#restartCronetEngine here}.
 * @param <T> used to describe the input of the Action.
 */
public interface Action<T> {
    default void configureBuilder(ActionData data, T value) {}
}
