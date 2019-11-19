// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/activity_icon_loader.h"

#include <string.h>

#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace arc {
namespace internal {

namespace {

constexpr size_t kSmallIconSizeInDip = 16;
constexpr size_t kLargeIconSizeInDip = 20;
constexpr size_t kMaxIconSizeInPx = 200;
constexpr char kPngDataUrlPrefix[] = "data:image/png;base64,";

ui::ScaleFactor GetSupportedScaleFactor() {
  std::vector<ui::ScaleFactor> scale_factors = ui::GetSupportedScaleFactors();
  DCHECK(!scale_factors.empty());
  return scale_factors.back();
}

// Returns an instance for calling RequestActivityIcons().
mojom::IntentHelperInstance* GetInstanceForRequestActivityIcons(
    ActivityIconLoader::GetResult* out_error_code) {
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
      if (out_error_code) {
        *out_error_code =
            ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
      }
    } else {
      VLOG(2) << "ARC bridge is not ready.";
      if (out_error_code)
        *out_error_code = ActivityIconLoader::GetResult::FAILED_ARC_NOT_READY;
    }
    return nullptr;
  }

  auto* intent_helper_holder =
      arc_service_manager->arc_bridge_service()->intent_helper();
  if (!intent_helper_holder->IsConnected()) {
    VLOG(2) << "ARC intent helper instance is not ready.";
    if (out_error_code)
      *out_error_code = ActivityIconLoader::GetResult::FAILED_ARC_NOT_READY;
    return nullptr;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder,
                                               RequestActivityIcons);
  if (!instance && out_error_code)
    *out_error_code = ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
  return instance;
}

// Encodes the |image| as PNG data considering scale factor, and returns it as
// data: URL.
scoped_refptr<base::RefCountedData<GURL>> GeneratePNGDataUrl(
    const gfx::ImageSkia& image,
    ui::ScaleFactor scale_factor) {
  float scale = ui::GetScaleForScaleFactor(scale_factor);
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(image.GetRepresentation(scale).GetBitmap(),
                                    false /* discard_transparency */, &output);
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(output.data()),
                        output.size()),
      &encoded);
  return base::WrapRefCounted(
      new base::RefCountedData<GURL>(GURL(kPngDataUrlPrefix + encoded)));
}

std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> ResizeAndEncodeIcons(
    std::vector<mojom::ActivityIconPtr> icons,
    ui::ScaleFactor scale_factor) {
  auto result = std::make_unique<ActivityIconLoader::ActivityToIconsMap>();
  for (size_t i = 0; i < icons.size(); ++i) {
    static const size_t kBytesPerPixel = 4;
    const mojom::ActivityIconPtr& icon = icons.at(i);
    if (icon->width > kMaxIconSizeInPx || icon->height > kMaxIconSizeInPx ||
        icon->width == 0 || icon->height == 0 ||
        icon->icon.size() != (icon->width * icon->height * kBytesPerPixel)) {
      continue;
    }

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(icon->width, icon->height));
    if (!bitmap.getPixels())
      continue;
    DCHECK_GE(bitmap.computeByteSize(), icon->icon.size());
    memcpy(bitmap.getPixels(), &icon->icon.front(), icon->icon.size());

    gfx::ImageSkia original(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

    // Resize the original icon to the sizes intent_helper needs.
    gfx::ImageSkia icon_large(gfx::ImageSkiaOperations::CreateResizedImage(
        original, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kLargeIconSizeInDip, kLargeIconSizeInDip)));
    gfx::ImageSkia icon_small(gfx::ImageSkiaOperations::CreateResizedImage(
        original, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kSmallIconSizeInDip, kSmallIconSizeInDip)));
    gfx::Image icon16(icon_small);
    gfx::Image icon20(icon_large);

    const std::string activity_name = icon->activity->activity_name.has_value()
                                          ? (*icon->activity->activity_name)
                                          : std::string();
    result->insert(std::make_pair(
        ActivityIconLoader::ActivityName(icon->activity->package_name,
                                         activity_name),
        ActivityIconLoader::Icons(
            icon16, icon20, GeneratePNGDataUrl(icon_small, scale_factor))));
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

bool ActivityIconLoader::ActivityName::operator<(
    const ActivityName& other) const {
  return std::tie(package_name, activity_name) <
         std::tie(other.package_name, other.activity_name);
}

ActivityIconLoader::ActivityIconLoader()
    : scale_factor_(GetSupportedScaleFactor()) {}

ActivityIconLoader::~ActivityIconLoader() = default;

void ActivityIconLoader::InvalidateIcons(const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto it = cached_icons_.begin(); it != cached_icons_.end();) {
    if (it->first.package_name == package_name)
      it = cached_icons_.erase(it);
    else
      ++it;
  }
}

ActivityIconLoader::GetResult ActivityIconLoader::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    OnIconsReadyCallback cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::unique_ptr<ActivityToIconsMap> result(new ActivityToIconsMap);
  std::vector<mojom::ActivityNamePtr> activities_to_fetch;

  for (const auto& activity : activities) {
    const auto& it = cached_icons_.find(activity);
    if (it == cached_icons_.end()) {
      mojom::ActivityNamePtr name(mojom::ActivityName::New());
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

  GetResult error_code;
  auto* instance = GetInstanceForRequestActivityIcons(&error_code);
  if (!instance) {
    // The mojo channel is not yet ready (or not supported at all). Run the
    // callback with |result| that could be empty.
    std::move(cb).Run(std::move(result));
    return error_code;
  }

  // Fetch icons from ARC.
  instance->RequestActivityIcons(
      std::move(activities_to_fetch), mojom::ScaleFactor(scale_factor_),
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
    std::vector<mojom::ActivityIconPtr> icons) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ResizeAndEncodeIcons, std::move(icons), scale_factor_),
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
