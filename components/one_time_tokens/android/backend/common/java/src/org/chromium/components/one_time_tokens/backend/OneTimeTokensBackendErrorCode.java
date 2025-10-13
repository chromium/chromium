// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.one_time_tokens.backend;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Needs to be kept in sync with OneTimeTokensBackendErrorCode in
// tools/metrics/histograms/enums.xml.
@IntDef({OneTimeTokensBackendErrorCode.GMSCORE_VERSION_NOT_SUPPORTED})
@Retention(RetentionPolicy.SOURCE)
public @interface OneTimeTokensBackendErrorCode {
    int GMSCORE_VERSION_NOT_SUPPORTED = 0;
    int MAX_VALUE = GMSCORE_VERSION_NOT_SUPPORTED;
}
