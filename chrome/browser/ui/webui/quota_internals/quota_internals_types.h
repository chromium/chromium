// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_TYPES_H_
#define CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_TYPES_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/time/time.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace quota_internals {

// Represents global usage and quota information for specific type of storage.
class GlobalStorageInfo {
 public:
  explicit GlobalStorageInfo(blink::mojom::StorageType type);
  ~GlobalStorageInfo();

  void set_usage(int64_t usage) { usage_ = usage; }

  void set_unlimited_usage(int64_t unlimited_usage) {
    unlimited_usage_ = unlimited_usage;
  }

  void set_quota(int64_t quota) { quota_ = quota; }

  // Create new Value for passing to WebUI page.
  std::unique_ptr<base::Value> NewValue() const;

 private:
  blink::mojom::StorageType type_;

  int64_t usage_;
  int64_t unlimited_usage_;
  int64_t quota_;
};

// Represents per host usage and quota information for the storage.
class PerHostStorageInfo {
 public:
  PerHostStorageInfo(const std::string& host, blink::mojom::StorageType type);
  ~PerHostStorageInfo();

  void set_usage(int64_t usage) { usage_ = usage; }

  void set_quota(int64_t quota) { quota_ = quota; }

  // Create new Value for passing to WebUI page.
  std::unique_ptr<base::Value> NewValue() const;

 private:
  std::string host_;
  blink::mojom::StorageType type_;

  int64_t usage_;
  int64_t quota_;
};

// Represents per origin usage and access time information.
class PerOriginStorageInfo {
 public:
  PerOriginStorageInfo(const GURL& origin, blink::mojom::StorageType type);
  PerOriginStorageInfo(const PerOriginStorageInfo& other);
  ~PerOriginStorageInfo();

  void set_used_count(int used_count) {
    used_count_ = used_count;
  }

  void set_last_access_time(base::Time last_access_time) {
    last_access_time_ = last_access_time;
  }

  void set_last_modified_time(base::Time last_modified_time) {
    last_modified_time_ = last_modified_time;
  }

  // Create new Value for passing to WebUI page.
  std::unique_ptr<base::Value> NewValue() const;

 private:
  GURL origin_;
  blink::mojom::StorageType type_;
  std::string host_;

  int used_count_;
  base::Time last_access_time_;
  base::Time last_modified_time_;
};
}  // namespace quota_internals

#endif  // CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_TYPES_H_
