// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_

#include <memory>

#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"

class GURL;

namespace commerce {

struct ProductInfo;

// Returns a mojo ProductInfo for use in IPC constructed from the shopping
// service's ProductInfo.
shopping_service::mojom::ProductInfoPtr ProductInfoToMojoProduct(
    const GURL& url,
    const std::optional<const ProductInfo>& info,
    const std::string& locale);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_
