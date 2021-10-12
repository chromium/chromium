// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/about_this_site_service.h"

#include "components/optimization_guide/core/optimization_metadata.h"
#include "url/gurl.h"

namespace page_info {

AboutThisSiteService::AboutThisSiteService(std::unique_ptr<Client> client)
    : client_(std::move(client)) {}

absl::optional<page_info::proto::SiteInfo>
AboutThisSiteService::GetAboutThisSiteInfo(const GURL& url) const {
  optimization_guide::OptimizationMetadata metadata;
  client_->CanApplyOptimization(url, &metadata);
  absl::optional<page_info::proto::AboutThisSiteMetadata>
      about_this_site_metadata =
          metadata.ParsedMetadata<page_info::proto::AboutThisSiteMetadata>();
  // TODO(crbug.com/1250653): Add validation to check if proto contains any
  // useful data.
  if (about_this_site_metadata) {
    return about_this_site_metadata->site_info();
  }

  // TODO(crbug.com/1250653): Remove returning fake data after server-side is
  // ready.
  page_info::proto::SiteInfo site_info_metadata;
  const GURL test_gurl("https://example.com");
  if (url == test_gurl) {
    site_info_metadata.set_entity_description(
        "A domain used in illustrative examples in documents");
    return site_info_metadata;
  }

  const GURL permissions_gurl("https://permission.site");
  if (url == permissions_gurl) {
    site_info_metadata.set_entity_description(
        "A site containing test buttons for various browser APIs, in order"
        " to trigger permission dialogues and similar UI in modern "
        "browsers.");
    return site_info_metadata;
  }

  return absl::nullopt;
}

AboutThisSiteService::~AboutThisSiteService() = default;

}  // namespace page_info
