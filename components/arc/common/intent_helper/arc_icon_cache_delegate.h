// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_ICON_CACHE_DELEGATE_H_
#define COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_ICON_CACHE_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/arc/common/intent_helper/activity_icon_loader.h"

namespace arc {

// This class stores activity icon cache for ARC and provides API to handle and
// access to the cache.
class ArcIconCacheDelegate {
 public:
  virtual ~ArcIconCacheDelegate();

  // internal::ActivityIconLoader types.
  using ActivityIconLoader = internal::ActivityIconLoader;
  using ActivityName = internal::ActivityIconLoader::ActivityName;
  using ActivityToIconsMap = internal::ActivityIconLoader::ActivityToIconsMap;
  using GetResult = internal::ActivityIconLoader::GetResult;
  using OnIconsReadyCallback =
      internal::ActivityIconLoader::OnIconsReadyCallback;

  // Return ArcIconCacheDelegate instance.
  static ArcIconCacheDelegate* GetInstance();

  // Retrieves icons for the |activities| and calls |callback|.
  // See internal::ActivityIconLoader::GetActivityIcons() for more details.
  virtual GetResult GetActivityIcons(
      const std::vector<ActivityName>& activities,
      OnIconsReadyCallback callback) = 0;
};

// Provides ArcIconCacheDelegate implementation.
class ArcIconCacheDelegateProvider {
 public:
  explicit ArcIconCacheDelegateProvider(ArcIconCacheDelegate* delegate);
  ArcIconCacheDelegateProvider(const ArcIconCacheDelegateProvider&) = delete;
  ArcIconCacheDelegateProvider& operator=(const ArcIconCacheDelegateProvider&) =
      delete;
  ~ArcIconCacheDelegateProvider();

  ArcIconCacheDelegate* GetInstance();

 private:
  raw_ptr<ArcIconCacheDelegate> delegate_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_ICON_CACHE_DELEGATE_H_
