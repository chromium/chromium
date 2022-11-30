// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_hints/common/switches.h"

namespace client_hints {
namespace switches {

// Pre-load the client hint storage. Takes a JSON dict, with each key being an
// origin (RFC 6454 Section 6.2) and each value a comma-separated list of client
// hint tokens (RFC 8942 Section 3.1, client-hints-infrastructure Section 7.1).
//
// Each origin/token-list entry will be parsed and persisted to the Client Hints
// storage as though the token-list had come through an Accept-CH response
// header from a navigation from the origin.
//
// The initialization will only apply to non-OffTheRecord profiles, meaning
// incognito or guest profiles will not have the storage applied.
const char kInitializeClientHintsStorage[] = "initialize-client-hints-storage";

}  // namespace switches
}  // namespace client_hints