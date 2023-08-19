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

std::vector<OsRegistrationItem> ParseOsSourceOrTriggerHeader(
    base::StringPiece header) {
  const auto list = net::structured_headers::ParseList(header);
  if (!list) {
    return {};
  }

  return ParseOsSourceOrTriggerHeader(*list);
}

std::vector<OsRegistrationItem> ParseOsSourceOrTriggerHeader(
    const net::structured_headers::List& list) {
  std::vector<OsRegistrationItem> items;
  items.reserve(list.size());

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

    bool debug_reporting = false;
    for (const auto& param : parameterized_member.params) {
      if (param.first == "debug-reporting") {
        if (param.second.is_boolean()) {
          debug_reporting = param.second.GetBoolean();
        }
        break;
      }
    }

    items.emplace_back(OsRegistrationItem{
        .url = std::move(url),
        .debug_reporting = debug_reporting,
    });
  }

  return items;
}

}  // namespace attribution_reporting
