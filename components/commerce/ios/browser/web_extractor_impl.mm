// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/commerce/ios/browser/web_extractor_impl.h"

#import "base/json/json_reader.h"
#import "base/strings/utf_string_conversions.h"
#import "components/grit/components_resources.h"
#import "ui/base/resource/resource_bundle.h"

namespace commerce {

WebExtractorImpl::WebExtractorImpl() = default;
WebExtractorImpl::~WebExtractorImpl() = default;

void WebExtractorImpl::ExtractMetaInfo(
    WebWrapper* web_wrapper,
    base::OnceCallback<void(base::Value)> callback) {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_QUERY_SHOPPING_META_JS);

  web_wrapper->RunJavascript(
      base::UTF8ToUTF16(script),
      base::BindOnce(&WebExtractorImpl::OnExtractionMetaInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebExtractorImpl::OnExtractionMetaInfo(
    base::OnceCallback<void(base::Value)> callback,
    base::Value result) {
  const std::string* json = result.GetIfString();
  if (!json) {
    std::move(callback).Run(base::Value());
    return;
  }

  std::optional<base::Value> val =
      base::JSONReader::Read(*json, base::JSON_PARSE_RFC);
  if (val && val->is_dict()) {
    std::move(callback).Run(std::move(*val));
  } else {
    std::move(callback).Run(base::Value());
  }
}

}  // namespace commerce
