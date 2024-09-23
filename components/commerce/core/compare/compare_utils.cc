// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/compare_utils.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace commerce {
namespace {
const char kTypeKey[] = "type";
const char kGPCTypeName[] = "GLOBAL_PRODUCT_CLUSTER_ID";
const char kIdentifierKey[] = "identifier";
const char kProductIdsKey[] = "productIds";
}  // namespace

std::string GetJsonStringForProductClusterIds(
    std::vector<uint64_t> product_cluster_ids) {
  base::Value::List product_id_list;
  for (uint64_t id : product_cluster_ids) {
    base::Value::Dict id_definition;
    id_definition.Set(kTypeKey, kGPCTypeName);
    id_definition.Set(kIdentifierKey, base::NumberToString(id));
    product_id_list.Append(std::move(id_definition));
  }

  base::Value::Dict json_dict;
  json_dict.Set(kProductIdsKey, std::move(product_id_list));
  std::string post_data;
  base::JSONWriter::Write(json_dict, &post_data);
  return post_data;
}

}  // namespace commerce
