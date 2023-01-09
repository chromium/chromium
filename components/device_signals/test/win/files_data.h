// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_TEST_WIN_FILES_DATA_H_
#define COMPONENTS_DEVICE_SIGNALS_TEST_WIN_FILES_DATA_H_

namespace device_signals::test {

// Base64-encoded bytes of a signed test executable.
extern const char kSignedExeBase64[];

// Base64-encoded bytes of a test executable signed with two certificates.
extern const char kMultiSignedExeBase64[];

// Base64-encoded bytes of a test executable that has metadata information baked
// into it (e.g. product name, version).
extern const char kMetadataExeBase64[];

// Base64-encoded bytes of an empty test executable (no metadata, no signature).
extern const char kEmptyExeBase64[];

}  // namespace device_signals::test

#endif  // COMPONENTS_DEVICE_SIGNALS_TEST_WIN_FILES_DATA_H_
