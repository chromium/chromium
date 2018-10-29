// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/origin_policy_ui.h"

#include "components/grit/components_resources.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace security_interstitials {

base::Optional<std::string> OriginPolicyUI::GetErrorPage(
    content::OriginPolicyErrorReason error_reason,
    const url::Origin& origin,
    const GURL& url) {
  const base::StringPiece raw_interstitial_page_resource =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SECURITY_INTERSTITIAL_ORIGIN_POLICY_HTML);

  // The resource is gzip compressed.
  std::string interstitial_page_resource;
  interstitial_page_resource.resize(
      compression::GetUncompressedSize(raw_interstitial_page_resource));
  base::StringPiece buffer(interstitial_page_resource.c_str(),
                           interstitial_page_resource.size());
  CHECK(compression::GzipUncompress(raw_interstitial_page_resource, buffer));

  ui::TemplateReplacements params;
  params["url"] = url.spec();
  params["origin"] = origin.Serialize();

  return ui::ReplaceTemplateExpressions(interstitial_page_resource, params);
}

}  // namespace security_interstitials
