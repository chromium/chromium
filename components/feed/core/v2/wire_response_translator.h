// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WIRE_RESPONSE_TRANSLATOR_H_
#define COMPONENTS_FEED_CORE_V2_WIRE_RESPONSE_TRANSLATOR_H_

#include "components/feed/core/v2/protocol_translator.h"

namespace feed {
struct AccountInfo;

// Forwards to |feed::TranslateWireResponse()| by default. Can be overridden
// for testing.
class WireResponseTranslator {
 public:
  WireResponseTranslator() = default;
  ~WireResponseTranslator() = default;
  virtual RefreshResponseData TranslateWireResponse(
      feedwire::Response response,
      StreamModelUpdateRequest::Source source,
      const AccountInfo& account_info,
      base::Time current_time) const;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WIRE_RESPONSE_TRANSLATOR_H_
