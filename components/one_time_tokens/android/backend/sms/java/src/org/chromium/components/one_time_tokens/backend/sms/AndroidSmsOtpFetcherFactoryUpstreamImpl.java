// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.one_time_tokens.backend.sms;

import org.chromium.build.annotations.NullMarked;

/**
 * Instantiable version of {@link AndroidSmsOtpFetcherFactory}, don't add anything to this class.
 * Downstream provides an actual implementation via ServiceLoader/@ServiceImpl.
 */
@NullMarked
class AndroidSmsOtpFetcherFactoryUpstreamImpl extends AndroidSmsOtpFetcherFactory {}
