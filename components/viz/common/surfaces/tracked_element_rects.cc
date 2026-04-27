// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/tracked_element_rects.h"

#include <sstream>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace viz {

std::string TrackedElementRect::ToString() const {
  return base::StrCat(
      {"{id: ", id.ToString(), ", visible_bounds: ", visible_bounds.ToString(),
       ", frame_token: ",
       frame_token.has_value() ? frame_token->ToString() : "null",
       ", parent_frame_token: ",
       parent_frame_token.has_value() ? parent_frame_token->ToString() : "null",
       "}"});
}

std::string TrackedElementRectsToString(const TrackedElementRects& bounds) {
  std::vector<std::string> element_strings;
  for (const auto& [feature, vector] : bounds) {
    std::string s = base::StrCat(
        {"feature: ", base::NumberToString(static_cast<int32_t>(feature)),
         ", rects: ["});
    for (auto it = vector.begin(); it != vector.end(); ++it) {
      if (it != vector.begin()) {
        s += ", ";
      }
      s += it->ToString();
    }
    s += "]";
    element_strings.push_back(s);
  }
  return base::JoinString(element_strings, "; ");
}

const TrackedElementRects& TrackedElementRectsEmpty() {
  static const base::NoDestructor<TrackedElementRects> empty_rects;
  return *empty_rects;
}

}  // namespace viz
