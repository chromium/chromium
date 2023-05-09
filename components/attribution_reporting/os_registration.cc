// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/os_registration.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "net/http/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace attribution_reporting {

std::vector<GURL> ParseOsSourceOrTriggerHeader(base::StringPiece header) {
  const auto list = net::structured_headers::ParseList(header);
  if (!list) {
    return {};
  }

  return ParseOsSourceOrTriggerHeader(*list);
}

std::vector<GURL> ParseOsSourceOrTriggerHeader(
    const net::structured_headers::List& list) {
  std::vector<GURL> urls;
  urls.reserve(list.size());

  for (const auto& parameterized_member : list) {
    if (parameterized_member.member_is_inner_list) {
      continue;
    }

    DCHECK_EQ(parameterized_member.member.size(), 1u);
    const auto& parameterized_item = parameterized_member.member.front();

    if (!parameterized_item.item.is_string()) {
      continue;
    }

    GURL url(parameterized_item.item.GetString());
    if (!url.is_valid()) {
      continue;
    }

    urls.emplace_back(std::move(url));
  }

  return urls;
}

}  // namespace attribution_reporting
