// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/commerce/ios/browser/web_extractor_impl.h"

#import "base/strings/utf_string_conversions.h"
#import "components/grit/components_resources.h"
#import "ui/base/resource/resource_bundle.h"

namespace commerce {

WebExtractorImpl::WebExtractorImpl() = default;
WebExtractorImpl::~WebExtractorImpl() = default;

void WebExtractorImpl::ExtractMetaInfo(
    WebWrapper* web_wrapper,
    base::OnceCallback<void(const base::Value)> callback) {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_QUERY_SHOPPING_META_JS);

  web_wrapper->RunJavascript(base::UTF8ToUTF16(script), std::move(callback));
}

}  // namespace commerce
