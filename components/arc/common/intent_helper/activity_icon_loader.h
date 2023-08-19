// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_
#define COMPONENTS_ARC_COMMON_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/arc.mojom.h"  // nogncheck
#else
#error "ARC files should only be included for Ash-chrome or Lacros-chrome."
#endif

namespace arc {

class AdaptiveIconDelegate;

namespace internal {

// A class which retrieves an activity icon from ARC.
class ActivityIconLoader {
 public:
  struct Icons {
    Icons(const gfx::Image& icon16,
          const gfx::Image& icon20,
          const scoped_refptr<base::RefCountedData<GURL>>& icon16_dataurl);
    Icons(const Icons& other);
    ~Icons();

    const gfx::Image icon16;                                         // 16 dip
    const gfx::Image icon20;                                         // 20 dip
    const scoped_refptr<base::RefCountedData<GURL>> icon16_dataurl;  // as URL
  };

  struct ActivityName {
    ActivityName(const std::string& package_name,
                 const std::string& activity_name);
    ~ActivityName();
    bool operator<(const ActivityName& other) const;

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

  // Ash uses arc::mojom interface while Lacros uses crosapi::mojom.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  using ActivityIconPtr = mojom::ActivityIconPtr;
  using ActivityNamePtr = mojom::ActivityNamePtr;
  using ScaleFactor = mojom::ScaleFactor;
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  using ActivityIconPtr = crosapi::mojom::ActivityIconPtr;
  using ActivityNamePtr = crosapi::mojom::ActivityNamePtr;
  using ScaleFactor = crosapi::mojom::ScaleFactor;
#endif

  using ActivityToIconsMap = std::map<ActivityName, Icons>;
  using OnIconsReadyCallback =
      base::OnceCallback<void(std::unique_ptr<ActivityToIconsMap>)>;

  ActivityIconLoader();
  ActivityIconLoader(const ActivityIconLoader&) = delete;
  ActivityIconLoader& operator=(const ActivityIconLoader&) = delete;
  ~ActivityIconLoader();

  void SetAdaptiveIconDelegate(AdaptiveIconDelegate* delegate);

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

  void OnIconsReadyForTesting(std::unique_ptr<ActivityToIconsMap> cached_result,
                              OnIconsReadyCallback cb,
                              std::vector<ActivityIconPtr> icons);

  // Returns true if |result| indicates that the |cb| object passed to
  // GetActivityIcons() has already called.
  static bool HasIconsReadyCallbackRun(GetResult result);

  const ActivityToIconsMap& cached_icons_for_testing() { return cached_icons_; }

 private:
  // A function called when the mojo IPC returns.
  void OnIconsReady(std::unique_ptr<ActivityToIconsMap> cached_result,
                    OnIconsReadyCallback cb,
                    std::vector<ActivityIconPtr> icons);

  // A function called when the adaptive icons are generated.
  void OnAdaptiveIconGenerated(
      std::vector<ActivityName> actvity_names,
      std::unique_ptr<ActivityToIconsMap> cached_result,
      OnIconsReadyCallback cb,
      const std::vector<gfx::ImageSkia>& adaptive_icons);

  // A function called when ResizeIcons finishes. Append items in |result| to
  // |cached_icons_|.
  void OnIconsResized(std::unique_ptr<ActivityToIconsMap> cached_result,
                      OnIconsReadyCallback cb,
                      std::unique_ptr<ActivityToIconsMap> result);

  // The maximum scale factor the current platform supports.
  const ui::ResourceScaleFactor scale_factor_;
  // A map which holds icons in a scale-factor independent form (gfx::Image).
  ActivityToIconsMap cached_icons_;

  // A delegate which converts the icon to the adaptive icon.
  raw_ptr<AdaptiveIconDelegate> delegate_ = nullptr;

  THREAD_CHECKER(thread_checker_);

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ActivityIconLoader> weak_ptr_factory_{this};
};

}  // namespace internal
}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_
