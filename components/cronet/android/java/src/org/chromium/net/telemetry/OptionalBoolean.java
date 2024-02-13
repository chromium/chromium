// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

/**
 * The generated CronetStatsLog class has an optionalBoolean(UNSET,TRUE,FALSE) variable for each of
 * the experimental options. Since these values will always be the same for the options, we picked
 * one of them and used it to create a private variable that we can use to make the code more
 * readable.
 */
public enum OptionalBoolean {
    UNSET(
            CronetStatsLog
                    .CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_STORE_SERVER_CONFIGS_IN_PROPERTIES__OPTIONAL_BOOLEAN_UNSET),
    TRUE(
            CronetStatsLog
                    .CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_STORE_SERVER_CONFIGS_IN_PROPERTIES__OPTIONAL_BOOLEAN_TRUE),
    FALSE(
            CronetStatsLog
                    .CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_STORE_SERVER_CONFIGS_IN_PROPERTIES__OPTIONAL_BOOLEAN_FALSE);

    private final int mValue;

    private OptionalBoolean(int value) {
        this.mValue = value;
    }

    public int getValue() {
        return mValue;
    }

    public static OptionalBoolean fromBoolean(Boolean value) {
        if (value == null) {
            return UNSET;
        }

        return value ? TRUE : FALSE;
    }
}
