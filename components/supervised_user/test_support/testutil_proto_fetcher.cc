// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/testutil_proto_fetcher.h"

#include <memory>
#include <string>

#include "components/supervised_user/core/browser/proto_fetcher.h"

namespace supervised_user {

template <>
void TypedFetchProcess<std::string>::OnResponse(
    std::optional<std::string> response_body) {
  CHECK(response_body) << "Use OnError when there is no response.";
  OnSuccess(std::make_unique<std::string>(std::move(response_body.value())));
}

}  // namespace supervised_user
