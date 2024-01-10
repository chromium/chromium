// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_

#include <memory>

#include "components/commerce/core/mojom/shopping_list.mojom.h"

class GURL;

namespace commerce {

struct ProductInfo;

// Returns a mojo ProductInfo for use in IPC constructed from the shopping
// service's ProductInfo.
shopping_list::mojom::ProductInfoPtr ProductInfoToMojoProduct(
    const GURL& url,
    const absl::optional<const ProductInfo>& info,
    const std::string& locale);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_
