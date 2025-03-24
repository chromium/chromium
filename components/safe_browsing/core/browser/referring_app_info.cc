// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/referring_app_info.h"

#include "components/safe_browsing/core/browser/utils/safe_browsing_web_app_utils.h"

namespace safe_browsing {
namespace internal {

ReferringAppInfo::ReferringAppInfo() = default;

ReferringAppInfo::~ReferringAppInfo() = default;

ReferringAppInfo::ReferringAppInfo(const ReferringAppInfo&) = default;

ReferringAppInfo::ReferringAppInfo(ReferringAppInfo&&) = default;

ReferringAppInfo& ReferringAppInfo::operator=(const ReferringAppInfo&) =
    default;

ReferringAppInfo& ReferringAppInfo::operator=(ReferringAppInfo&&) = default;

GetReferringAppInfoResult ReferringAppInfoToResult(
    const ReferringAppInfo& info) {
  using Result = GetReferringAppInfoResult;
  return info.has_referring_app()
             ? (info.has_referring_webapk()
                    ? Result::kReferringAppFoundWebapkFound
                    : Result::kReferringAppFoundWebapkNotFound)
             : (info.has_referring_webapk()
                    ? Result::kReferringAppNotFoundWebapkFound
                    : Result::kReferringAppNotFoundWebapkNotFound);
}

}  // namespace internal

ReferringAppInfo GetReferringAppInfoProto(
    const internal::ReferringAppInfo& info) {
  ReferringAppInfo referring_app_info_proto;
  if (info.has_referring_app()) {
    referring_app_info_proto.set_referring_app_name(info.referring_app_name);
    referring_app_info_proto.set_referring_app_source(
        info.referring_app_source);
  }
  if (info.has_referring_webapk()) {
    std::optional<SafeBrowsingWebAppKey> webapk = GetSafeBrowsingWebAppKey(
        info.referring_webapk_start_url, info.referring_webapk_manifest_id);
    if (webapk) {
      *referring_app_info_proto.mutable_referring_webapk() = std::move(*webapk);
    }
  }
  return referring_app_info_proto;
}

}  // namespace safe_browsing
