// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/arc/intent.h"
#include "components/crx_file/id_util.h"

namespace arc {

namespace {

// Prefix in intent that specifies ARC shelf group. S. means string type.
constexpr char kShelfGroupIntentPrefix[] = "S.org.chromium.arc.shelf_group_id=";
// Prefix to specify ARC shelf group.
constexpr char kShelfGroupPrefix[] = "shelf_group:";

}  // namespace

ArcAppShelfId::ArcAppShelfId() = default;

ArcAppShelfId::ArcAppShelfId(const std::string& shelf_group_id,
                             const std::string& app_id)
    : shelf_group_id_(shelf_group_id), app_id_(app_id) {
  DCHECK(crx_file::id_util::IdIsValid(app_id));
}

ArcAppShelfId::ArcAppShelfId(const ArcAppShelfId& other) = default;

ArcAppShelfId::~ArcAppShelfId() = default;

// static
ArcAppShelfId ArcAppShelfId::FromString(const std::string& id) {
  if (base::StartsWith(id, kShelfGroupPrefix, base::CompareCase::SENSITIVE)) {
    const std::vector<std::string> parts = base::SplitString(
        id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() == 3u && crx_file::id_util::IdIsValid(parts[2]))
      return ArcAppShelfId(parts[1], parts[2]);
  } else if (crx_file::id_util::IdIsValid(id)) {
    return ArcAppShelfId(std::string(), id);
  }
  return ArcAppShelfId();
}

// static
ArcAppShelfId ArcAppShelfId::FromIntentAndAppId(const std::string& intent,
                                                const std::string& app_id) {
  if (intent.empty())
    return ArcAppShelfId(std::string(), app_id);

  auto parsed_intent = Intent::Get(intent);
  if (!parsed_intent)
    return ArcAppShelfId(std::string(), app_id);

  const std::string prefix(kShelfGroupIntentPrefix);
  for (const auto& param : parsed_intent->extra_params()) {
    if (base::StartsWith(param, prefix, base::CompareCase::SENSITIVE))
      return ArcAppShelfId(param.substr(prefix.length()), app_id);
  }

  return ArcAppShelfId(std::string(), app_id);
}

std::string ArcAppShelfId::ToString() const {
  if (!has_shelf_group_id())
    return app_id_;

  return base::StringPrintf("%s%s:%s", kShelfGroupPrefix,
                            shelf_group_id_.c_str(), app_id_.c_str());
}

bool ArcAppShelfId::operator<(const ArcAppShelfId& other) const {
  const int compare_group = shelf_group_id_.compare(other.shelf_group_id());
  if (compare_group == 0)
    return app_id_ < other.app_id();
  return compare_group < 0;
}

}  // namespace arc
