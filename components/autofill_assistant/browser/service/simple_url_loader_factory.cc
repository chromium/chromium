// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"

namespace autofill_assistant {

std::unique_ptr<::network::SimpleURLLoader>
NativeURLLoaderFactory::CreateLoader(
    std::unique_ptr<::network::ResourceRequest> resource_request,
    const ::net::NetworkTrafficAnnotationTag& annotation_tag) const {
  return ::network::SimpleURLLoader::Create(std::move(resource_request),
                                            annotation_tag);
}

}  // namespace autofill_assistant
