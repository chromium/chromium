// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_EXTRACTOR_IMPL_H_
#define COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_EXTRACTOR_IMPL_H_

#include "components/commerce/core/mojom/commerce_web_extractor.mojom.h"
#include "components/commerce/core/web_extractor.h"
#include "components/commerce/core/web_wrapper.h"
#include "mojo/public/cpp/bindings/remote.h"

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
      mojo::Remote<commerce_web_extractor::mojom::CommerceWebExtractor>
          extractor,
      base::OnceCallback<void(const base::Value)> callback,
      const base::Value result);

  base::WeakPtrFactory<WebExtractorImpl> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CONTENT_BROWSER_WEB_EXTRACTOR_IMPL_H_
