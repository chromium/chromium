// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GOOGLE_CORE_COMMON_GOOGLE_SWITCHES_H_
#define COMPONENTS_GOOGLE_CORE_COMMON_GOOGLE_SWITCHES_H_

namespace switches {

// All switches in alphabetical order.

// Specifies an alternate suggestion URL to use for speaking to Google. Useful
// for testing.
inline constexpr char kGoogleBaseSuggestionURL[] = "google-base-suggestion-url";

// Specifies an alternate URL to use for speaking to Google. Useful for testing.
inline constexpr char kGoogleBaseURL[] = "google-base-url";

// When set, this will ignore the PortPermission passed in the google_util.h
// methods and ignore the port numbers. This makes it easier to run tests for
// features that use these methods (directly or indirectly) with the
// EmbeddedTestServer, which is more representative of production.
inline constexpr char kIgnoreGooglePortNumbers[] = "ignore-google-port-numbers";

}  // namespace switches

#endif  // COMPONENTS_GOOGLE_CORE_COMMON_GOOGLE_SWITCHES_H_
