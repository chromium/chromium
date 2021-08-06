// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals/quota_internals_types.h"

#include <utility>

#include "base/check.h"
#include "base/values.h"
#include "net/base/url_util.h"

namespace {

std::string StorageTypeToString(blink::mojom::StorageType type) {
  switch (type) {
    case blink::mojom::StorageType::kTemporary:
      return "temporary";
    case blink::mojom::StorageType::kPersistent:
      return "persistent";
    case blink::mojom::StorageType::kSyncable:
      return "syncable";
    case blink::mojom::StorageType::kQuotaNotManaged:
      return "quota not managed";
    case blink::mojom::StorageType::kUnknown:
      return "unknown";
  }
  return "unknown";
}

}  // anonymous namespace

namespace quota_internals {

GlobalStorageInfo::GlobalStorageInfo(blink::mojom::StorageType type)
    : type_(type), usage_(-1), unlimited_usage_(-1), quota_(-1) {}

GlobalStorageInfo::~GlobalStorageInfo() {}

std::unique_ptr<base::Value> GlobalStorageInfo::NewValue() const {
  // TODO(tzik): Add CreateLongIntegerValue to base/values.h and remove
  // all static_cast<double> in this file.
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("type", StorageTypeToString(type_));
  if (usage_ >= 0)
    dict->SetDoubleKey("usage", static_cast<double>(usage_));
  if (unlimited_usage_ >= 0)
    dict->SetDoubleKey("unlimitedUsage", static_cast<double>(unlimited_usage_));
  if (quota_ >= 0)
    dict->SetDoubleKey("quota", static_cast<double>(quota_));
  return std::move(dict);
}

PerHostStorageInfo::PerHostStorageInfo(const std::string& host,
                                       blink::mojom::StorageType type)
    : host_(host), type_(type), usage_(-1), quota_(-1) {}

PerHostStorageInfo::~PerHostStorageInfo() {}

std::unique_ptr<base::Value> PerHostStorageInfo::NewValue() const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  DCHECK(!host_.empty());
  dict->SetString("host", host_);
  dict->SetString("type", StorageTypeToString(type_));
  if (usage_ >= 0)
    dict->SetDoubleKey("usage", static_cast<double>(usage_));
  if (quota_ >= 0)
    dict->SetDoubleKey("quota", static_cast<double>(quota_));
  return std::move(dict);
}

PerOriginStorageInfo::PerOriginStorageInfo(const GURL& origin,
                                           blink::mojom::StorageType type)
    : origin_(origin),
      type_(type),
      host_(origin.host()),
      used_count_(-1) {}

PerOriginStorageInfo::PerOriginStorageInfo(const PerOriginStorageInfo& other) =
    default;

PerOriginStorageInfo::~PerOriginStorageInfo() {}

std::unique_ptr<base::Value> PerOriginStorageInfo::NewValue() const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  DCHECK(!origin_.is_empty());
  DCHECK(!host_.empty());
  dict->SetString("origin", origin_.spec());
  dict->SetString("type", StorageTypeToString(type_));
  dict->SetString("host", host_);
  if (used_count_ >= 0)
    dict->SetInteger("usedCount", used_count_);
  if (!last_access_time_.is_null())
    dict->SetDoubleKey("lastAccessTime",
                       last_access_time_.ToDoubleT() * 1000.0);
  if (!last_modified_time_.is_null()) {
    dict->SetDoubleKey("lastModifiedTime",
                       last_modified_time_.ToDoubleT() * 1000.0);
  }
  return std::move(dict);
}

}  // namespace quota_internals
