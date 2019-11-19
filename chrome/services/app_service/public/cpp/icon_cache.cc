// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/icon_cache.h"

#include <utility>

#include "base/callback.h"

namespace apps {

IconCache::Value::Value()
    : image_(), is_placeholder_icon_(false), ref_count_(0) {}

apps::mojom::IconValuePtr IconCache::Value::AsIconValue() {
  auto icon_value = apps::mojom::IconValue::New();
  icon_value->icon_compression = apps::mojom::IconCompression::kUncompressed;
  icon_value->uncompressed = image_;
  icon_value->is_placeholder_icon = is_placeholder_icon_;
  return icon_value;
}

IconCache::IconCache(IconLoader* wrapped_loader,
                     GarbageCollectionPolicy gc_policy)
    : wrapped_loader_(wrapped_loader), gc_policy_(gc_policy) {}

IconCache::~IconCache() = default;

apps::mojom::IconKeyPtr IconCache::GetIconKey(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return wrapped_loader_ ? wrapped_loader_->GetIconKey(app_id)
                         : apps::mojom::IconKey::New();
}

std::unique_ptr<IconLoader::Releaser> IconCache::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IconLoader::Key key(
      app_type, app_id, icon_key, icon_compression, size_hint_in_dip,
      // We pass false instead of allow_placeholder_icon, as the Value
      // already records placeholder-ness. If the allow_placeholder_icon
      // arg to this function is true, we can re-use a cache hit regardless
      // of whether the previous call to the underlying wrapped_loader_
      // returned the placeholder icon or the real icon, so we don't want
      // to restrict our map lookup to only one flavor.
      false);
  Value* cache_hit = nullptr;
  bool ref_count_incremented = false;

  if (icon_compression == apps::mojom::IconCompression::kUncompressed) {
    auto iter = map_.find(key);
    if (iter == map_.end()) {
      iter = map_.insert(std::make_pair(key, Value())).first;
    } else if (!iter->second.image_.isNull() &&
               (allow_placeholder_icon || !iter->second.is_placeholder_icon_)) {
      cache_hit = &iter->second;
    }

    iter->second.ref_count_++;
    ref_count_incremented = true;
  }

  std::unique_ptr<IconLoader::Releaser> releaser(nullptr);
  if (cache_hit) {
    std::move(callback).Run(cache_hit->AsIconValue());
  } else if (wrapped_loader_) {
    releaser = wrapped_loader_->LoadIconFromIconKey(
        app_type, app_id, std::move(icon_key), icon_compression,
        size_hint_in_dip, allow_placeholder_icon,
        base::BindOnce(&IconCache::OnLoadIcon, weak_ptr_factory_.GetWeakPtr(),
                       key, std::move(callback)));
  } else {
    std::move(callback).Run(apps::mojom::IconValue::New());
  }

  return ref_count_incremented
             ? std::make_unique<IconLoader::Releaser>(
                   std::move(releaser),
                   base::BindOnce(&IconCache::OnRelease,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(key)))
             : std::move(releaser);
}

void IconCache::SweepReleasedIcons() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (gc_policy_ != GarbageCollectionPolicy::kExplicit) {
    return;
  }

  auto iter = map_.begin();
  while (iter != map_.end()) {
    if (iter->second.ref_count_ == 0) {
      iter = map_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void IconCache::Update(const IconLoader::Key& key,
                       const apps::mojom::IconValue& icon_value) {
  if (icon_value.icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }

  auto iter = map_.find(key);
  if (iter == map_.end()) {
    return;
  }

  // Don't let a placeholder overwrite a real icon.
  if (icon_value.is_placeholder_icon && !iter->second.is_placeholder_icon_) {
    return;
  }

  iter->second.image_ = icon_value.uncompressed;
}

void IconCache::OnLoadIcon(
    IconLoader::Key key,
    apps::mojom::Publisher::LoadIconCallback wrapped_callback,
    apps::mojom::IconValuePtr icon_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Update(key, *icon_value);
  std::move(wrapped_callback).Run(std::move(icon_value));
}

void IconCache::OnRelease(IconLoader::Key key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = map_.find(key);
  if (iter == map_.end()) {
    NOTREACHED();
    return;
  }

  uint64_t n = iter->second.ref_count_;
  DCHECK(n > 0);
  n--;
  iter->second.ref_count_ = n;

  if ((n == 0) && (gc_policy_ == GarbageCollectionPolicy::kEager)) {
    map_.erase(iter);
  }
}

}  // namespace apps
