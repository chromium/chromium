// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_IOS_BROWSER_WEB_EXTRACTOR_IMPL_H_
#define COMPONENTS_COMMERCE_IOS_BROWSER_WEB_EXTRACTOR_IMPL_H_

#include "components/commerce/core/web_extractor.h"
#include "components/commerce/core/web_wrapper.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace commerce {

class WebExtractorImpl : public WebExtractor {
 public:
  WebExtractorImpl();
  WebExtractorImpl(const WebExtractorImpl&) = delete;
  WebExtractorImpl operator=(const WebExtractorImpl&) = delete;
  ~WebExtractorImpl() override;

  // commerce::WebExtractor implementation.
  void ExtractMetaInfo(
      WebWrapper* web_wrapper,
      base::OnceCallback<void(const base::Value)> callback) override;

 private:
  void OnExtractionMetaInfo(
      base::OnceCallback<void(const base::Value)> callback,
      const base::Value result);

  void OnProductInfoJsonSanitizationCompleted(
      base::OnceCallback<void(const base::Value)> callback,
      data_decoder::DataDecoder::ValueOrError result);

  base::WeakPtrFactory<WebExtractorImpl> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_IOS_BROWSER_WEB_EXTRACTOR_IMPL_H_
