// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_SODA_UTIL_H_
#define COMPONENTS_SODA_SODA_UTIL_H_

namespace speech {

// Returns whether the Speech On-Device API is supported in
// Chrome. This can depend on e.g. Chrome feature flags, platform/OS, supported
// CPU instructions.
bool IsOnDeviceSpeechRecognitionSupported();

}  // namespace speech

#endif  // COMPONENTS_SODA_SODA_UTIL_H_
