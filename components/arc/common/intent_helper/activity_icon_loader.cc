// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/intent_helper/activity_icon_loader.h"

#include <string.h>

#include <string_view>
#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "components/arc/common/intent_helper/adaptive_icon_delegate.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace arc {
namespace internal {

namespace {

constexpr size_t kSmallIconSizeInDip = 16;
constexpr size_t kLargeIconSizeInDip = 20;
constexpr size_t kMaxIconSizeInPx = 200;
constexpr char kPngDataUrlPrefix[] = "data:image/png;base64,";

// Returns an instance for calling RequestActivityIcons().
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Ash requests icons to ArcServiceManager.
absl::variant<mojom::IntentHelperInstance*, ActivityIconLoader::GetResult>
GetInstanceForRequestActivityIcons() {
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager) {
    // TODO(hidehiko): IsArcAvailable() looks not the condition to be checked
    // here, because ArcServiceManager instance is created regardless of ARC
    // availability. This happens only before MessageLoop starts or after
    // MessageLoop stops, practically.
    // Also, returning FAILED_ARC_NOT_READY looks problematic at the moment,
    // because ArcProcessTask::StartIconLoading accesses to
    // ArcServiceManager::Get() return value, which can be nullptr.
    if (!IsArcAvailable()) {
      VLOG(2) << "ARC bridge is not supported.";
      return ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
    }

    VLOG(2) << "ARC bridge is not ready.";
    return ActivityIconLoader::GetResult::FAILED_ARC_NOT_READY;
  }

  auto* intent_helper_holder =
      arc_service_manager->arc_bridge_service()->intent_helper();
  if (!intent_helper_holder->IsConnected()) {
    VLOG(2) << "ARC intent helper instance is not ready.";
    return ActivityIconLoader::GetResult::FAILED_ARC_NOT_READY;
  }

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, RequestActivityIcons);
  if (!instance) {
    return ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
  }
  return instance;
}
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
// Adapter class for wrapping crosapi::mojom::Arc used in lacros-chrome.
// This is returned from GetInstanceForRequestActivityIcons().
class Adapter {
 public:
  explicit Adapter(crosapi::mojom::Arc* instance) : instance_(instance) {}
  ~Adapter() = default;

  using OnRequestActivityIconsSucceededCallback =
      base::OnceCallback<void(std::vector<crosapi::mojom::ActivityIconPtr>)>;

  // If status is not kSuccess, immediately return callback.
  void RequestActivityIcons(
      std::vector<crosapi::mojom::ActivityNamePtr> activities,
      crosapi::mojom::ScaleFactor scale_factor,
      OnRequestActivityIconsSucceededCallback cb) {
    instance_->RequestActivityIcons(
        std::move(activities), scale_factor,
        base::BindOnce(
            [](OnRequestActivityIconsSucceededCallback cb,
               std::vector<crosapi::mojom::ActivityIconPtr> icons,
               crosapi::mojom::RequestActivityIconsStatus status) {
              // If status is not kSuccess, immediately return callback.
              if (status == crosapi::mojom::RequestActivityIconsStatus::
                                kArcNotAvailable) {
                LOG(ERROR) << "Failed to connect to ARC in ash-chrome.";
                std::move(cb).Run(
                    std::vector<crosapi::mojom::ActivityIconPtr>());
                return;
              }

              std::move(cb).Run(std::move(icons));
            },
            std::move(cb)));
  }

 private:
  raw_ptr<crosapi::mojom::Arc> instance_;
};

// Lacros requests icons to ash-chrome via crosapi.
absl::variant<std::unique_ptr<Adapter>, ActivityIconLoader::GetResult>
GetInstanceForRequestActivityIcons() {
  auto* service = chromeos::LacrosService::Get();

  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    VLOG(2) << "ARC is not supported in Lacros.";
    return ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
  }

  if (service->GetInterfaceVersion<crosapi::mojom::Arc>() <
      int{crosapi::mojom::Arc::MethodMinVersions::
              kRequestActivityIconsMinVersion}) {
    VLOG(2) << "Ash Lacros-Arc version "
            << service->GetInterfaceVersion<crosapi::mojom::Arc>()
            << " does not support RequestActivityIcons().";
    return ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
  }

  return std::make_unique<Adapter>(
      service->GetRemote<crosapi::mojom::Arc>().get());
}

#endif

ActivityIconLoader::ActivityName GenerateActivityName(
    const ActivityIconLoader::ActivityIconPtr& icon) {
  return ActivityIconLoader::ActivityName(
      icon->activity->package_name, icon->activity->activity_name.has_value()
                                        ? (*icon->activity->activity_name)
                                        : std::string());
}

// Encodes the |image| as PNG data considering scale factor, and returns it as
// data: URL.
scoped_refptr<base::RefCountedData<GURL>> GeneratePNGDataUrl(
    const gfx::ImageSkia& image,
    ui::ResourceScaleFactor scale_factor) {
  float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(image.GetRepresentation(scale).GetBitmap(),
                                    false /* discard_transparency */, &output);
  const std::string encoded = base::Base64Encode(std::string_view(
      reinterpret_cast<const char*>(output.data()), output.size()));
  return base::WrapRefCounted(
      new base::RefCountedData<GURL>(GURL(kPngDataUrlPrefix + encoded)));
}

ActivityIconLoader::Icons ResizeIconsInternal(
    const gfx::ImageSkia& image,
    ui::ResourceScaleFactor scale_factor) {
  // Resize the original icon to the sizes intent_helper needs.
  gfx::ImageSkia icon_large(gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kLargeIconSizeInDip, kLargeIconSizeInDip)));
  icon_large.MakeThreadSafe();
  gfx::Image icon20(icon_large);

  gfx::ImageSkia icon_small(gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kSmallIconSizeInDip, kSmallIconSizeInDip)));
  icon_small.MakeThreadSafe();
  gfx::Image icon16(icon_small);

  return ActivityIconLoader::Icons(
      icon16, icon20, GeneratePNGDataUrl(icon_small, scale_factor));
}

std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> ResizeAndEncodeIcons(
    std::vector<ActivityIconLoader::ActivityIconPtr> icons,
    ui::ResourceScaleFactor scale_factor) {
  auto result = std::make_unique<ActivityIconLoader::ActivityToIconsMap>();
  for (size_t i = 0; i < icons.size(); ++i) {
    static const size_t kBytesPerPixel = 4;
    const ActivityIconLoader::ActivityIconPtr& icon = icons.at(i);
    if (icon->width > kMaxIconSizeInPx || icon->height > kMaxIconSizeInPx ||
        icon->width == 0 || icon->height == 0 ||
        icon->icon.size() != (icon->width * icon->height * kBytesPerPixel)) {
      continue;
    }

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(icon->width, icon->height));
    if (!bitmap.getPixels()) {
      continue;
    }
    DCHECK_GE(bitmap.computeByteSize(), icon->icon.size());
    memcpy(bitmap.getPixels(), &icon->icon.front(), icon->icon.size());

    gfx::ImageSkia original(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

    result->insert(std::make_pair(GenerateActivityName(icon),
                                  ResizeIconsInternal(original, scale_factor)));
  }

  return result;
}

std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> ResizeIcons(
    std::vector<ActivityIconLoader::ActivityName> activity_names,
    const std::vector<gfx::ImageSkia>& images,
    ui::ResourceScaleFactor scale_factor) {
  DCHECK_EQ(activity_names.size(), images.size());
  auto result = std::make_unique<ActivityIconLoader::ActivityToIconsMap>();
  for (size_t i = 0; i < activity_names.size(); ++i) {
    result->insert(std::make_pair(
        activity_names[i], ResizeIconsInternal(images[i], scale_factor)));
  }

  return result;
}

}  // namespace

ActivityIconLoader::Icons::Icons(
    const gfx::Image& icon16,
    const gfx::Image& icon20,
    const scoped_refptr<base::RefCountedData<GURL>>& icon16_dataurl)
    : icon16(icon16), icon20(icon20), icon16_dataurl(icon16_dataurl) {}

ActivityIconLoader::Icons::Icons(const Icons& other) = default;

ActivityIconLoader::Icons::~Icons() = default;

ActivityIconLoader::ActivityName::ActivityName(const std::string& package_name,
                                               const std::string& activity_name)
    : package_name(package_name), activity_name(activity_name) {}

ActivityIconLoader::ActivityName::~ActivityName() = default;

bool ActivityIconLoader::ActivityName::operator<(
    const ActivityName& other) const {
  return std::tie(package_name, activity_name) <
         std::tie(other.package_name, other.activity_name);
}

ActivityIconLoader::ActivityIconLoader()
    : scale_factor_(ui::GetMaxSupportedResourceScaleFactor()) {}

ActivityIconLoader::~ActivityIconLoader() = default;

void ActivityIconLoader::SetAdaptiveIconDelegate(
    AdaptiveIconDelegate* delegate) {
  delegate_ = delegate;
}

void ActivityIconLoader::InvalidateIcons(const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto it = cached_icons_.begin(); it != cached_icons_.end();) {
    if (it->first.package_name == package_name) {
      it = cached_icons_.erase(it);
    } else {
      ++it;
    }
  }
}

ActivityIconLoader::GetResult ActivityIconLoader::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    OnIconsReadyCallback cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::unique_ptr<ActivityToIconsMap> result(new ActivityToIconsMap);
  std::vector<ActivityNamePtr> activities_to_fetch;

  for (const auto& activity : activities) {
    const auto& it = cached_icons_.find(activity);
    if (it == cached_icons_.end()) {
      ActivityNamePtr name(ActivityNamePtr::Struct::New());
      name->package_name = activity.package_name;
      name->activity_name = activity.activity_name;
      activities_to_fetch.push_back(std::move(name));
    } else {
      result->insert(std::make_pair(activity, it->second));
    }
  }

  if (activities_to_fetch.empty()) {
    // If there's nothing to fetch, run the callback now.
    std::move(cb).Run(std::move(result));
    return GetResult::SUCCEEDED_SYNC;
  }

  auto instance = GetInstanceForRequestActivityIcons();
  if (absl::holds_alternative<GetResult>(instance)) {
    // The mojo channel is not yet ready (or not supported at all). Run the
    // callback with |result| that could be empty.
    std::move(cb).Run(std::move(result));
    return absl::get<GetResult>(instance);
  }

  // Fetch icons from ARC.
  absl::get<0>(instance)->RequestActivityIcons(
      std::move(activities_to_fetch), ScaleFactor(scale_factor_),
      base::BindOnce(&ActivityIconLoader::OnIconsReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result),
                     std::move(cb)));
  return GetResult::SUCCEEDED_ASYNC;
}

void ActivityIconLoader::OnIconsResizedForTesting(
    OnIconsReadyCallback cb,
    std::unique_ptr<ActivityToIconsMap> result) {
  OnIconsResized(std::make_unique<ActivityToIconsMap>(), std::move(cb),
                 std::move(result));
}

void ActivityIconLoader::AddCacheEntryForTesting(const ActivityName& activity) {
  cached_icons_.insert(
      std::make_pair(activity, Icons(gfx::Image(), gfx::Image(), nullptr)));
}

void ActivityIconLoader::OnIconsReadyForTesting(
    std::unique_ptr<ActivityToIconsMap> cached_result,
    OnIconsReadyCallback cb,
    std::vector<ActivityIconPtr> icons) {
  OnIconsReady(std::move(cached_result), std::move(cb), std::move(icons));
}

// static
bool ActivityIconLoader::HasIconsReadyCallbackRun(GetResult result) {
  switch (result) {
    case GetResult::SUCCEEDED_ASYNC:
      return false;
    case GetResult::SUCCEEDED_SYNC:
    case GetResult::FAILED_ARC_NOT_READY:
    case GetResult::FAILED_ARC_NOT_SUPPORTED:
      break;
  }
  return true;
}

void ActivityIconLoader::OnIconsReady(
    std::unique_ptr<ActivityToIconsMap> cached_result,
    OnIconsReadyCallback cb,
    std::vector<ActivityIconPtr> icons) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    std::vector<ActivityName> actvity_names;
    for (const auto& icon : icons)
      actvity_names.emplace_back(GenerateActivityName(icon));

    delegate_->GenerateAdaptiveIcons(
        icons,
        base::BindOnce(&ActivityIconLoader::OnAdaptiveIconGenerated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(actvity_names),
                       std::move(cached_result), std::move(cb)));
    return;
  }

  // TODO(crbug.com/40131344): Remove when the adaptive icon feature is enabled
  // by default.
  // TODO(crbug.com/40806186): Adaptive Icon is not supported in Lacros now. Do
  // not remove this until it's supported.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ResizeAndEncodeIcons, std::move(icons), scale_factor_),
      base::BindOnce(&ActivityIconLoader::OnIconsResized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cached_result),
                     std::move(cb)));
}

void ActivityIconLoader::OnAdaptiveIconGenerated(
    std::vector<ActivityName> actvity_names,
    std::unique_ptr<ActivityToIconsMap> cached_result,
    OnIconsReadyCallback cb,
    const std::vector<gfx::ImageSkia>& adaptive_icons) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ResizeIcons, std::move(actvity_names), adaptive_icons,
                     scale_factor_),
      base::BindOnce(&ActivityIconLoader::OnIconsResized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cached_result),
                     std::move(cb)));
}

void ActivityIconLoader::OnIconsResized(
    std::unique_ptr<ActivityToIconsMap> cached_result,
    OnIconsReadyCallback cb,
    std::unique_ptr<ActivityToIconsMap> result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Update |cached_icons_|.
  for (const auto& kv : *result) {
    cached_icons_.erase(kv.first);
    cached_icons_.insert(std::make_pair(kv.first, kv.second));
  }

  // Merge the results that were obtained from cache before doing IPC.
  result->insert(cached_result->begin(), cached_result->end());
  std::move(cb).Run(std::move(result));
}

}  // namespace internal
}  // namespace arc
