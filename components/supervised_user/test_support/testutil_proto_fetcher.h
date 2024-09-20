// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_TESTUTIL_PROTO_FETCHER_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_TESTUTIL_PROTO_FETCHER_H_

#include <memory>
#include <string>

#include "components/supervised_user/core/browser/proto_fetcher.h"

namespace supervised_user {

// std::string specialization for fetchers that are not interested in parsing
// the message.
template <>
void TypedFetchProcess<std::string>::OnResponse(
    std::unique_ptr<std::string> response_body);

}  // namespace supervised_user
#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_TESTUTIL_PROTO_FETCHER_H_
