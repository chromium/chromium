// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TEST_FAKE_SERVER_RESPONSE_H_
#define COMPONENTS_QUERY_TILES_TEST_FAKE_SERVER_RESPONSE_H_

#include <memory>
#include <string>

#include "url/gurl.h"

namespace query_tiles {

// This class provides the necessary utilities that can be used to fake the
// query tiles server interaction for using in tests.
class FakeServerResponse {
 public:
  // Sets the query tile server endpoint to the given |url|.
  static void SetTileFetcherServerURL(const GURL& url);

  // Creates a fake server response proto, which has |levels| tiers, and each
  // tier has |tiles_per_level| tiles.
  static std::string CreateServerResponseProto(int levels, int tiles_per_level);
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_TEST_FAKE_SERVER_RESPONSE_H_
