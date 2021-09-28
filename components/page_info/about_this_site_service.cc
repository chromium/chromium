// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/about_this_site_service.h"

#include "components/optimization_guide/core/optimization_metadata.h"
#include "url/gurl.h"

namespace page_info {

AboutThisSiteService::AboutThisSiteService(std::unique_ptr<Client> client)
    : client_(std::move(client)) {}

std::u16string AboutThisSiteService::GetAboutThisSiteDescription(
    const GURL& url) const {
  // TODO(crbug.com/1250653): Return the actual data after server-side is ready
  // and proto is added.
  const GURL test_gurl("https://example.com");
  if (url == test_gurl) {
    return u"A domain used in illustrative examples in documents";
  }

  const GURL permissions_gurl("https://permission.site");
  if (url == permissions_gurl) {
    return u"A site containing test buttons for various browser APIs, in order"
           " to trigger permission dialogues and similar UI in modern "
           "browsers.";
  }

  return std::u16string();
}

AboutThisSiteService::~AboutThisSiteService() = default;

}  // namespace page_info
