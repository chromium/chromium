// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_create_info.h"

#include <memory>
#include <string>

#include "base/format_macros.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_response_headers.h"

namespace download {

DownloadCreateInfo::DownloadCreateInfo(
    const base::Time& start_time,
    std::unique_ptr<DownloadSaveInfo> save_info)
    : is_new_download(true),
      referrer_policy(
          net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      start_time(start_time),
      total_bytes(0),
      offset(0),
      has_user_gesture(false),
      transient(false),
      require_safety_checks(true),
      result(DOWNLOAD_INTERRUPT_REASON_NONE),
      save_info(std::move(save_info)),
      render_process_id(-1),
      render_frame_id(-1),
      accept_range(RangeRequestSupportType::kNoSupport),
      connection_info(net::HttpConnectionInfo::kUNKNOWN),
      method("GET"),
      ukm_source_id(ukm::kInvalidSourceId),
      is_content_initiated(false),
      credentials_mode(::network::mojom::CredentialsMode::kInclude),
      isolation_info(std::nullopt) {}

DownloadCreateInfo::DownloadCreateInfo()
    : DownloadCreateInfo(base::Time(), std::make_unique<DownloadSaveInfo>()) {}

DownloadCreateInfo::~DownloadCreateInfo() = default;

const GURL& DownloadCreateInfo::url() const {
  return url_chain.empty() ? GURL::EmptyGURL() : url_chain.back();
}

}  // namespace download
