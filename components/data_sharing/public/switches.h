// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_SWITCHES_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_SWITCHES_H_

namespace data_sharing {

// Switch for whether or not data sharing logs are stored at startup instead of
// only being stored/logged when chrome://data-sharing-internals is open.
inline constexpr char kDataSharingDebugLoggingEnabled[] =
    "data-sharing-debug-logs";

}  // namespace data_sharing
#endif  // COMPONENTS_DATA_SHARING_PUBLIC_SWITCHES_H_
