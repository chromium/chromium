// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEB_EXTRACTOR_H_
#define COMPONENTS_COMMERCE_CORE_WEB_EXTRACTOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/web_wrapper.h"

namespace commerce {

// The class for extracting commerce-related information locally from
// `WebWrapper`.
class WebExtractor {
 public:
  WebExtractor();
  WebExtractor(const WebExtractor&) = delete;
  virtual ~WebExtractor();

  // Extract commerce-related meta info from `web_wrapper` and return the
  // results via `callback`.
  virtual void ExtractMetaInfo(
      WebWrapper* web_wrapper,
      base::OnceCallback<void(base::Value)> callback) = 0;

 private:
  base::WeakPtrFactory<WebExtractor> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEB_EXTRACTOR_H_
