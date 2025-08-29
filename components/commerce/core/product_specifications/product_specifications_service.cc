// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"

namespace commerce {

const size_t kMaxNameLength = 64;
const size_t kMaxTableSize = 10;

ProductSpecificationsService::ProductSpecificationsService() = default;

ProductSpecificationsService::~ProductSpecificationsService() = default;

const std::vector<ProductSpecificationsSet>
ProductSpecificationsService::GetAllProductSpecifications() {
  return {};
}

void ProductSpecificationsService::GetAllProductSpecifications(
    GetAllCallback callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), GetAllProductSpecifications()));
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::GetSetByUuid(const base::Uuid& uuid) {
    return std::nullopt;
}

void ProductSpecificationsService::GetSetByUuid(
    const base::Uuid& uuid,
    base::OnceCallback<void(std::optional<ProductSpecificationsSet>)>
        callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetSetByUuid(uuid)));
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::AddProductSpecificationsSet(
    const std::string& name,
    const std::vector<UrlInfo>& url_infos) {
    return std::nullopt;
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::SetUrls(const base::Uuid& uuid,
                                      const std::vector<UrlInfo>& url_infos) {
  return std::nullopt;
}

const std::optional<ProductSpecificationsSet>
ProductSpecificationsService::SetName(const base::Uuid& uuid,
                                      const std::string& name) {
  return std::nullopt;
}

void ProductSpecificationsService::DeleteProductSpecificationsSet(
    const std::string& uuid) {
}

void ProductSpecificationsService::AddObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
}

void ProductSpecificationsService::RemoveObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
}

}  // namespace commerce
