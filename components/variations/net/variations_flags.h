// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_FLAGS_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_FLAGS_H_

namespace variations {

// A command line switch to append variation headers to localhost requests.
// This can be used in unittests.
extern const char kAppendVariationsHeadersToLocalhostForTesting[];

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_FLAGS_H_
