// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/grit/component_extension_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"

namespace {

bool disable_safe_decoding_for_testing = false;

std::string GetAppFromAppOrGroupId(content::BrowserContext* context,
                                   const std::string& app_or_group_id) {
  const arc::ArcAppShelfId app_shelf_id = arc::ArcAppShelfId::FromString(
      app_or_group_id);
  if (!app_shelf_id.has_shelf_group_id())
    return app_shelf_id.app_id();

  const ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(context);
  DCHECK(prefs);

  // Try to find a shortcut with requested shelf group id.
  const std::vector<std::string> app_ids = prefs->GetAppIds();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    DCHECK(app_info);
    if (!app_info || !app_info->shortcut)
      continue;
    const arc::ArcAppShelfId shortcut_shelf_id =
        arc::ArcAppShelfId::FromIntentAndAppId(app_info->intent_uri,
                                               app_id);
    if (shortcut_shelf_id.has_shelf_group_id() &&
        shortcut_shelf_id.shelf_group_id() == app_shelf_id.shelf_group_id()) {
      return app_id;
    }
  }

  // Shortcut with requested shelf group id was not found, use app id as
  // fallback.
  return app_shelf_id.app_id();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::ReadResult

struct ArcAppIcon::ReadResult {
  ReadResult(bool error,
             bool request_to_install,
             ui::ScaleFactor scale_factor,
             bool resize_allowed,
             std::string unsafe_icon_data)
      : error(error),
        request_to_install(request_to_install),
        scale_factor(scale_factor),
        resize_allowed(resize_allowed),
        unsafe_icon_data(unsafe_icon_data) {}

  const bool error;
  const bool request_to_install;
  const ui::ScaleFactor scale_factor;
  const bool resize_allowed;
  const std::string unsafe_icon_data;
};

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::Source

// Initializes the ImageSkia with placeholder bitmaps, decoded from
// compiled-into-the-binary resources such as IDR_APP_DEFAULT_ICON, and
// schedules the asynchronous loading of the app's actual bitmaps.
class ArcAppIcon::Source : public gfx::ImageSkiaSource {
 public:
  Source(const base::WeakPtr<ArcAppIcon>& host, int resource_size_in_dip);
  ~Source() override;

 private:
  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  // Used to load images asynchronously. NULLed out when the ArcAppIcon is
  // destroyed.
  base::WeakPtr<ArcAppIcon> host_;

  const int resource_size_in_dip_;

  // A map from a pair of a resource ID and size in DIP to an image. This
  // is a cache to avoid resizing IDR icons in GetImageForScale every time.
  static base::LazyInstance<std::map<std::pair<int, int>, gfx::ImageSkia>>::
      DestructorAtExit default_icons_cache_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

base::LazyInstance<std::map<std::pair<int, int>, gfx::ImageSkia>>::
    DestructorAtExit ArcAppIcon::Source::default_icons_cache_ =
        LAZY_INSTANCE_INITIALIZER;

ArcAppIcon::Source::Source(const base::WeakPtr<ArcAppIcon>& host,
                           int resource_size_in_dip)
    : host_(host),
      resource_size_in_dip_(resource_size_in_dip) {
}

ArcAppIcon::Source::~Source() {
}

gfx::ImageSkiaRep ArcAppIcon::Source::GetImageForScale(float scale) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Host loads icon asynchronously, so use default icon so far.
  int resource_id;
  if (host_ && host_->app_id() == arc::kPlayStoreAppId) {
    // Don't request icon from Android side. Use overloaded Chrome icon for Play
    // Store that is adapted according Chrome style.
    const int resource_size_in_px =
        static_cast<int>(resource_size_in_dip_ * scale + 0.5);
    resource_id = resource_size_in_px <= 32 ? IDR_ARC_SUPPORT_ICON_32
                                            : IDR_ARC_SUPPORT_ICON_192;
  } else {
    if (host_)
      host_->LoadForScaleFactor(ui::GetSupportedScaleFactor(scale));
    resource_id = IDR_APP_DEFAULT_ICON;
  }

  // Check |default_icons_cache_| and returns the existing one if possible.
  const auto key = std::make_pair(resource_id, resource_size_in_dip_);
  const auto it = default_icons_cache_.Get().find(key);
  if (it != default_icons_cache_.Get().end())
    return it->second.GetRepresentation(scale);

  const gfx::ImageSkia* default_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  CHECK(default_image);
  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      *default_image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(resource_size_in_dip_, resource_size_in_dip_));

  // Add the resized image to the cache to avoid executing the expensive resize
  // operation many times. Caching the result is safe because unlike ARC icons
  // that can be updated dynamically, IDR icons are static.
  default_icons_cache_.Get().insert(std::make_pair(key, resized_image));
  return resized_image.GetRepresentation(scale);
}

class ArcAppIcon::DecodeRequest : public ImageDecoder::ImageRequest {
 public:
  DecodeRequest(const base::WeakPtr<ArcAppIcon>& host,
                const ArcAppIconDescriptor& descriptor,
                bool resize_allowed);
  ~DecodeRequest() override;

  // ImageDecoder::ImageRequest
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;

 private:
  base::WeakPtr<ArcAppIcon> host_;
  const ArcAppIconDescriptor descriptor_;
  const bool resize_allowed_;

  DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
};

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::DecodeRequest

ArcAppIcon::DecodeRequest::DecodeRequest(const base::WeakPtr<ArcAppIcon>& host,
                                         const ArcAppIconDescriptor& descriptor,
                                         bool resize_allowed)
    : host_(host), descriptor_(descriptor), resize_allowed_(resize_allowed) {}

ArcAppIcon::DecodeRequest::~DecodeRequest() = default;

void ArcAppIcon::DecodeRequest::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(!bitmap.isNull() && !bitmap.empty());

  if (!host_)
    return;

  const int expected_dim = descriptor_.GetSizeInPixels();
  if (bitmap.width() != expected_dim || bitmap.height() != expected_dim) {
    if (!resize_allowed_) {
      VLOG(2) << "Decoded ARC icon has unexpected dimension " << bitmap.width()
              << "x" << bitmap.height() << ". Expected " << expected_dim << ".";
      host_->MaybeRequestIcon(descriptor_.scale_factor);
    } else {
      host_->UpdateUncompressed(descriptor_.scale_factor,
                                skia::ImageOperations::Resize(
                                    bitmap, skia::ImageOperations::RESIZE_BEST,
                                    expected_dim, expected_dim));
    }
  } else {
    host_->UpdateUncompressed(descriptor_.scale_factor, bitmap);
  }

  host_->DiscardDecodeRequest(this);
}

void ArcAppIcon::DecodeRequest::OnDecodeImageFailed() {
  VLOG(2) << "Failed to decode ARC icon.";

  if (!host_)
    return;

  host_->MaybeRequestIcon(descriptor_.scale_factor);
  host_->DiscardDecodeRequest(this);
}

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon

// static
void ArcAppIcon::DisableSafeDecodingForTesting() {
  disable_safe_decoding_for_testing = true;
}

// static
bool ArcAppIcon::IsSafeDecodingDisabledForTesting() {
  return disable_safe_decoding_for_testing;
}

ArcAppIcon::ArcAppIcon(content::BrowserContext* context,
                       const std::string& app_id,
                       int resource_size_in_dip,
                       Observer* observer,
                       bool serve_compressed_icons)
    : context_(context),
      app_id_(app_id),
      mapped_app_id_(GetAppFromAppOrGroupId(context, app_id)),
      resource_size_in_dip_(resource_size_in_dip),
      observer_(observer),
      serve_compressed_icons_(serve_compressed_icons) {
  CHECK(observer_ != nullptr);

  if (!serve_compressed_icons_) {
    auto source = std::make_unique<Source>(weak_ptr_factory_.GetWeakPtr(),
                                           resource_size_in_dip);
    gfx::Size resource_size(resource_size_in_dip, resource_size_in_dip);
    image_skia_ = gfx::ImageSkia(std::move(source), resource_size);
  }

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  for (const auto& scale_factor : scale_factors)
    incomplete_scale_factors_.insert({scale_factor, base::Time::Now()});
}

ArcAppIcon::~ArcAppIcon() {
}

void ArcAppIcon::LoadSupportedScaleFactors() {
  if (serve_compressed_icons_) {
    for (auto scale_factor : incomplete_scale_factors_)
      LoadForScaleFactor(scale_factor.first);
  } else {
    // Calling GetRepresentation indirectly calls LoadForScaleFactor but also
    // first initializes image_skia_ with the placeholder icons (e.g.
    // IDR_APP_DEFAULT_ICON), via ArcAppIcon::Source::GetImageForScale.
    for (auto scale_factor : incomplete_scale_factors_)
      image_skia_.GetRepresentation(
          ui::GetScaleForScaleFactor(scale_factor.first));
  }
}

bool ArcAppIcon::EverySupportedScaleFactorIsLoaded() const {
  return incomplete_scale_factors_.empty();
}

void ArcAppIcon::LoadForScaleFactor(ui::ScaleFactor scale_factor) {
  // We provide Play Store icon from Chrome resources and it is not expected
  // that we have external load request.
  DCHECK_NE(app_id(), arc::kPlayStoreAppId);

  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  const ArcAppIconDescriptor descriptor(resource_size_in_dip_, scale_factor);
  const base::FilePath path = prefs->GetIconPath(mapped_app_id_, descriptor);
  if (path.empty())
    return;

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          &ArcAppIcon::ReadOnFileThread, scale_factor, path,
          prefs->MaybeGetIconPathForDefaultApp(mapped_app_id_, descriptor)),
      base::BindOnce(&ArcAppIcon::OnIconRead, weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppIcon::MaybeRequestIcon(ui::ScaleFactor scale_factor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  // ArcAppListPrefs notifies the ArcAppIconLoader (which is an
  // ArcAppListPrefs::Observer) when the icon is updated, and
  // ArcAppIconLoader::OnAppIconUpdated calls ArcAppIcon::LoadForScaleFactor on
  // the corresponding ArcAppIcon.
  prefs->MaybeRequestIcon(
      mapped_app_id_,
      ArcAppIconDescriptor(resource_size_in_dip_, scale_factor));
}

// static
std::unique_ptr<ArcAppIcon::ReadResult> ArcAppIcon::ReadOnFileThread(
    ui::ScaleFactor scale_factor,
    const base::FilePath& path,
    const base::FilePath& default_app_path) {
  DCHECK(!path.empty());

  base::FilePath path_to_read;
  // Allow resizing only for default app icons.
  bool resize_allowed;
  if (base::PathExists(path)) {
    path_to_read = path;
    resize_allowed = false;
  } else {
    if (default_app_path.empty() || !base::PathExists(default_app_path)) {
      return std::make_unique<ArcAppIcon::ReadResult>(
          false /* error */, true /* request_to_install */, scale_factor,
          false /* resize_allowed */, std::string() /* unsafe_icon_data */);
    }
    path_to_read = default_app_path;
    resize_allowed = true;
  }

  bool request_to_install = path_to_read != path;

  // Read the file from disk.
  std::string unsafe_icon_data;
  if (!base::ReadFileToString(path_to_read, &unsafe_icon_data) ||
      unsafe_icon_data.empty()) {
    VLOG(2) << "Failed to read an ARC icon from file " << path.MaybeAsASCII();

    // If |unsafe_icon_data| is empty typically means we have a file corruption
    // on cached icon file. Send request to re install the icon.
    request_to_install |= unsafe_icon_data.empty();
    return std::make_unique<ArcAppIcon::ReadResult>(
        true /* error */, request_to_install, scale_factor,
        false /* resize_allowed */, std::string() /* unsafe_icon_data */);
  }

  return std::make_unique<ArcAppIcon::ReadResult>(
      false /* error */, request_to_install, scale_factor, resize_allowed,
      unsafe_icon_data);
}

void ArcAppIcon::OnIconRead(
    std::unique_ptr<ArcAppIcon::ReadResult> read_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (read_result->request_to_install)
    MaybeRequestIcon(read_result->scale_factor);

  if (!read_result->unsafe_icon_data.empty()) {
    if (serve_compressed_icons_) {
      UpdateCompressed(read_result->scale_factor,
                       std::move(read_result->unsafe_icon_data));
      return;
    }

    decode_requests_.emplace_back(std::make_unique<DecodeRequest>(
        weak_ptr_factory_.GetWeakPtr(),
        ArcAppIconDescriptor(resource_size_in_dip_, read_result->scale_factor),
        read_result->resize_allowed));
    if (disable_safe_decoding_for_testing) {
      SkBitmap bitmap;
      if (!read_result->unsafe_icon_data.empty() &&
          gfx::PNGCodec::Decode(
              reinterpret_cast<const unsigned char*>(
                  &read_result->unsafe_icon_data.front()),
              read_result->unsafe_icon_data.length(),
              &bitmap)) {
        decode_requests_.back()->OnImageDecoded(bitmap);
      } else {
        decode_requests_.back()->OnDecodeImageFailed();
      }
    } else {
      ImageDecoder::Start(decode_requests_.back().get(),
                          read_result->unsafe_icon_data);
    }
  }
}

void ArcAppIcon::UpdateUncompressed(ui::ScaleFactor scale_factor,
                                    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  gfx::ImageSkiaRep image_rep(bitmap, ui::GetScaleForScaleFactor(scale_factor));
  DCHECK(ui::IsSupportedScale(image_rep.scale()));

  image_skia_.RemoveRepresentation(image_rep.scale());
  image_skia_.AddRepresentation(image_rep);
  image_skia_.RemoveUnsupportedRepresentationsForScale(image_rep.scale());

  if (icon_loaded_count_++ < 5) {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.uncompressedFirst5",
        base::Time::Now() - incomplete_scale_factors_[scale_factor]);
  } else {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.uncompressedOthers",
        base::Time::Now() - incomplete_scale_factors_[scale_factor]);
  }
  incomplete_scale_factors_.erase(scale_factor);
  observer_->OnIconUpdated(this);
}

void ArcAppIcon::UpdateCompressed(ui::ScaleFactor scale_factor,
                                  std::string data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  compressed_images_[scale_factor] = std::move(data);

  if (icon_loaded_count_++ < 5) {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.compressedFirst5",
        base::Time::Now() - incomplete_scale_factors_[scale_factor]);
  } else {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.compressedOthers",
        base::Time::Now() - incomplete_scale_factors_[scale_factor]);
  }
  incomplete_scale_factors_.erase(scale_factor);
  observer_->OnIconUpdated(this);
}

void ArcAppIcon::DiscardDecodeRequest(DecodeRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = std::find_if(decode_requests_.begin(), decode_requests_.end(),
                         [request](const std::unique_ptr<DecodeRequest>& ptr) {
                           return ptr.get() == request;
                         });
  DCHECK(it != decode_requests_.end());
  decode_requests_.erase(it);
}
