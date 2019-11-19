// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_
#define COMPONENTS_ARC_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace arc {
namespace internal {

// A class which retrieves an activity icon from ARC.
class ActivityIconLoader {
 public:
  struct Icons {
    Icons(const gfx::Image& icon16, const gfx::Image& icon20,
          const scoped_refptr<base::RefCountedData<GURL>>& icon16_dataurl);
    Icons(const Icons& other);
    ~Icons();

    const gfx::Image icon16;  // 16 dip
    const gfx::Image icon20;  // 20 dip
    const scoped_refptr<base::RefCountedData<GURL>> icon16_dataurl;  // as URL
  };

  struct ActivityName {
    ActivityName(const std::string& package_name,
                 const std::string& activity_name);
    bool operator<(const ActivityName& other) const;

    // TODO(yusukes): Add const to these variables later. At this point,
    // doing so seems to confuse g++ 4.6 on builders.
    std::string package_name;

    // Can be empty. When |activity_name| is empty, the loader tries to fetch
    // the package's default icon.
    std::string activity_name;
  };

  enum class GetResult {
    // Succeeded. The callback will be called asynchronously.
    SUCCEEDED_ASYNC,
    // Succeeded. The callback has already been called synchronously.
    SUCCEEDED_SYNC,
    // Failed. The intent_helper instance is not yet ready. This is a temporary
    // error.
    FAILED_ARC_NOT_READY,
    // Failed. Either ARC is not supported at all or intent_helper instance
    // version is too old.
    FAILED_ARC_NOT_SUPPORTED,
  };

  using ActivityToIconsMap = std::map<ActivityName, Icons>;
  using OnIconsReadyCallback =
      base::OnceCallback<void(std::unique_ptr<ActivityToIconsMap>)>;

  ActivityIconLoader();
  ~ActivityIconLoader();

  // Removes icons associated with |package_name| from the cache.
  void InvalidateIcons(const std::string& package_name);

  // Retrieves icons for the |activities| and calls |cb|. The |cb| is called
  // back exactly once, either synchronously in the GetActivityIcons() when
  // the result is _not_ SUCCEEDED_ASYNC (i.e. all icons are already cached
  // locally or ARC is not ready/supported). Otherwise, the callback is run
  // later asynchronously with icons fetched from ARC side.
  GetResult GetActivityIcons(const std::vector<ActivityName>& activities,
                             OnIconsReadyCallback cb);

  void OnIconsResizedForTesting(OnIconsReadyCallback cb,
                                std::unique_ptr<ActivityToIconsMap> result);
  void AddCacheEntryForTesting(const ActivityName& activity);

  // Returns true if |result| indicates that the |cb| object passed to
  // GetActivityIcons() has already called.
  static bool HasIconsReadyCallbackRun(GetResult result);

  const ActivityToIconsMap& cached_icons_for_testing() { return cached_icons_; }

 private:
  // A function called when the mojo IPC returns.
  void OnIconsReady(std::unique_ptr<ActivityToIconsMap> cached_result,
                    OnIconsReadyCallback cb,
                    std::vector<mojom::ActivityIconPtr> icons);

  // A function called when ResizeIcons finishes. Append items in |result| to
  // |cached_icons_|.
  void OnIconsResized(std::unique_ptr<ActivityToIconsMap> cached_result,
                      OnIconsReadyCallback cb,
                      std::unique_ptr<ActivityToIconsMap> result);

  // The maximum scale factor the current platform supports.
  const ui::ScaleFactor scale_factor_;
  // A map which holds icons in a scale-factor independent form (gfx::Image).
  ActivityToIconsMap cached_icons_;

  THREAD_CHECKER(thread_checker_);

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ActivityIconLoader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ActivityIconLoader);
};

}  // namespace internal
}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_
