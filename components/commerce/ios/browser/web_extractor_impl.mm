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
  if (!result.is_string()) {
    std::move(callback).Run(base::Value());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      result.GetString(),
      base::BindOnce(&WebExtractorImpl::OnProductInfoJsonSanitizationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebExtractorImpl::OnProductInfoJsonSanitizationCompleted(
    base::OnceCallback<void(base::Value)> callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result.value().is_dict()) {
    std::move(callback).Run(base::Value());
    return;
  }

  std::move(callback).Run(std::move(result.value()));
}

}  // namespace commerce
