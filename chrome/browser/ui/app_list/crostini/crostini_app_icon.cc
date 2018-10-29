// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_icon.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"

////////////////////////////////////////////////////////////////////////////////
// CrostiniAppIcon::ReadResult

struct CrostiniAppIcon::ReadResult {
  ReadResult(bool error,
             bool request_to_install,
             ui::ScaleFactor scale_factor,
             std::string unsafe_icon_data)
      : error(error),
        request_to_install(request_to_install),
        scale_factor(scale_factor),
        unsafe_icon_data(unsafe_icon_data) {}

  bool error;
  bool request_to_install;
  ui::ScaleFactor scale_factor;
  std::string unsafe_icon_data;
};

////////////////////////////////////////////////////////////////////////////////
// CrostiniAppIcon::Source

class CrostiniAppIcon::Source : public gfx::ImageSkiaSource {
 public:
  Source(const base::WeakPtr<CrostiniAppIcon>& host, int resource_size_in_dip);
  ~Source() override;

 private:
  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  // Used to load images asynchronously. NULLed out when the CrostiniAppIcon is
  // destroyed.
  base::WeakPtr<CrostiniAppIcon> host_;

  const int resource_size_in_dip_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

CrostiniAppIcon::Source::Source(const base::WeakPtr<CrostiniAppIcon>& host,
                                int resource_size_in_dip)
    : host_(host), resource_size_in_dip_(resource_size_in_dip) {}

CrostiniAppIcon::Source::~Source() = default;

gfx::ImageSkiaRep CrostiniAppIcon::Source::GetImageForScale(float scale) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Host loads icon asynchronously, so use default icon so far.
  int resource_id;
  if (host_ && host_->app_id() == crostini::kCrostiniTerminalId) {
    // Don't initiate the icon request from the container because we have this
    // one already.
    resource_id = IDR_LOGO_CROSTINI_TERMINAL;
  } else {
    if (host_)
      host_->LoadForScaleFactor(ui::GetSupportedScaleFactor(scale));
    resource_id = IDR_LOGO_CROSTINI_DEFAULT;
  }

  // A map from a pair of a resource ID and size in DIP to an image. This
  // is a cache to avoid resizing IDR icons in GetImageForScale every time.
  static base::NoDestructor<std::map<std::pair<int, int>, gfx::ImageSkia>>
      default_icons_cache_;

  // Check |default_icons_cache_| and returns the existing one if possible.
  const auto key = std::make_pair(resource_id, resource_size_in_dip_);
  const auto it = default_icons_cache_->find(key);
  if (it != default_icons_cache_->end())
    return it->second.GetRepresentation(scale);

  const gfx::ImageSkia* default_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      *default_image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(resource_size_in_dip_, resource_size_in_dip_));

  // Add the resized image to the cache to avoid executing the expensive resize
  // operation many times. Caching the result is safe because unlike Crostini
  // icons that can be updated dynamically, IDR icons are static.
  default_icons_cache_->insert(std::make_pair(key, resized_image));
  return resized_image.GetRepresentation(scale);
}

class CrostiniAppIcon::DecodeRequest : public ImageDecoder::ImageRequest {
 public:
  DecodeRequest(const base::WeakPtr<CrostiniAppIcon>& host,
                int dimension,
                ui::ScaleFactor scale_factor);
  ~DecodeRequest() override;

  // ImageDecoder::ImageRequest
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;

 private:
  base::WeakPtr<CrostiniAppIcon> host_;
  int dimension_;
  ui::ScaleFactor scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
};

////////////////////////////////////////////////////////////////////////////////
// CrostiniAppIcon::DecodeRequest

CrostiniAppIcon::DecodeRequest::DecodeRequest(
    const base::WeakPtr<CrostiniAppIcon>& host,
    int dimension,
    ui::ScaleFactor scale_factor)
    : host_(host), dimension_(dimension), scale_factor_(scale_factor) {}

CrostiniAppIcon::DecodeRequest::~DecodeRequest() = default;

void CrostiniAppIcon::DecodeRequest::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(!bitmap.isNull() && !bitmap.empty());

  if (!host_)
    return;

  int expected_dim = static_cast<int>(
      ui::GetScaleForScaleFactor(scale_factor_) * dimension_ + 0.5f);
  if (bitmap.width() == expected_dim && bitmap.height() == expected_dim) {
    host_->Update(scale_factor_, bitmap);
    host_->DiscardDecodeRequest(this);
    return;
  }

  // TODO(jkardatzke): Remove this code for M72. This is a workaround to deal
  // with a bug where we were caching the wrong resolution of the icons and they
  // looked really bad. This only existed on dev channel, so there's limited
  // reach of it, and after everyone has upgraded we can remove this check.
  // crbug.com/891588
  if (bitmap.width() < expected_dim || bitmap.height() < expected_dim) {
    host_->MaybeRequestIcon(scale_factor_);
  }

  // We won't always get back from Crostini the icon size we asked for, so it
  // is expected that sometimes we need to rescale it.
  SkBitmap resized_image = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BEST, expected_dim, expected_dim);

  host_->Update(scale_factor_, std::move(resized_image));
  host_->DiscardDecodeRequest(this);
}

void CrostiniAppIcon::DecodeRequest::OnDecodeImageFailed() {
  VLOG(2) << "Failed to decode Crostini icon.";

  if (!host_)
    return;

  host_->MaybeRequestIcon(scale_factor_);
  host_->DiscardDecodeRequest(this);
}

////////////////////////////////////////////////////////////////////////////////
// CrostiniAppIcon

CrostiniAppIcon::CrostiniAppIcon(Profile* profile,
                                 const std::string& app_id,
                                 int resource_size_in_dip,
                                 Observer* observer)
    : registry_service_(
          crostini::CrostiniRegistryServiceFactory::GetForProfile(profile)
              ->GetWeakPtr()),
      app_id_(app_id),
      resource_size_in_dip_(resource_size_in_dip),
      observer_(observer),
      weak_ptr_factory_(this) {
  DCHECK_NE(observer_, nullptr);
  auto source = std::make_unique<Source>(weak_ptr_factory_.GetWeakPtr(),
                                         resource_size_in_dip);
  gfx::Size resource_size(resource_size_in_dip, resource_size_in_dip);
  image_skia_ = gfx::ImageSkia(std::move(source), resource_size);
}

CrostiniAppIcon::~CrostiniAppIcon() = default;

void CrostiniAppIcon::LoadForScaleFactor(ui::ScaleFactor scale_factor) {
  DCHECK(registry_service_);

  const base::FilePath path =
      registry_service_->GetIconPath(app_id_, scale_factor);
  DCHECK(!path.empty());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CrostiniAppIcon::ReadOnFileThread, scale_factor, path),
      base::BindOnce(&CrostiniAppIcon::OnIconRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniAppIcon::MaybeRequestIcon(ui::ScaleFactor scale_factor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Fail safely if the icon outlives the Profile (and the Crostini Registry).
  if (!registry_service_)
    return;

  // TODO(jkardatzke): Remove this for M-72, this is here temporarily to prevent
  // continually requesting updated icons if there are not larger size ones
  // available in Linux for that app.
  // crbug.com/891588
  if (already_requested_icons_.find(scale_factor) !=
      already_requested_icons_.end())
    return;
  already_requested_icons_.insert(scale_factor);

  // CrostiniRegistryService notifies CrostiniAppModelBuilder via Observer when
  // icon is ready and CrostiniAppModelBuilder refreshes the icon of the
  // corresponding item by calling LoadScaleFactor.
  registry_service_->MaybeRequestIcon(app_id_, scale_factor);
}

// static
std::unique_ptr<CrostiniAppIcon::ReadResult> CrostiniAppIcon::ReadOnFileThread(
    ui::ScaleFactor scale_factor,
    const base::FilePath& path) {
  DCHECK(!path.empty());

  if (!base::PathExists(path)) {
    return std::make_unique<CrostiniAppIcon::ReadResult>(
        false, true, scale_factor, std::string());
  }

  // Read the file from disk.
  std::string unsafe_icon_data;
  if (!base::ReadFileToString(path, &unsafe_icon_data) ||
      unsafe_icon_data.empty()) {
    VLOG(2) << "Failed to read a Crostini icon from file "
            << path.MaybeAsASCII();

    // If |unsafe_icon_data| is empty typically means we have a file corruption
    // on cached icon file. Send request to re install the icon.
    return std::make_unique<CrostiniAppIcon::ReadResult>(
        true, unsafe_icon_data.empty(), scale_factor, std::string());
  }

  return std::make_unique<CrostiniAppIcon::ReadResult>(
      false, false, scale_factor, unsafe_icon_data);
}

void CrostiniAppIcon::OnIconRead(
    std::unique_ptr<CrostiniAppIcon::ReadResult> read_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (read_result->request_to_install) {
    MaybeRequestIcon(read_result->scale_factor);
  }

  if (!read_result->unsafe_icon_data.empty()) {
    decode_requests_.push_back(std::make_unique<DecodeRequest>(
        weak_ptr_factory_.GetWeakPtr(), resource_size_in_dip_,
        read_result->scale_factor));
    ImageDecoder::Start(decode_requests_.back().get(),
                        read_result->unsafe_icon_data);
  }
}

void CrostiniAppIcon::Update(ui::ScaleFactor scale_factor,
                             const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  gfx::ImageSkiaRep image_rep(bitmap, ui::GetScaleForScaleFactor(scale_factor));
  DCHECK(ui::IsSupportedScale(image_rep.scale()));

  image_skia_.RemoveRepresentation(image_rep.scale());
  image_skia_.AddRepresentation(image_rep);
  image_skia_.RemoveUnsupportedRepresentationsForScale(image_rep.scale());

  observer_->OnIconUpdated(this);
}

void CrostiniAppIcon::DiscardDecodeRequest(DecodeRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = std::find_if(decode_requests_.begin(), decode_requests_.end(),
                         [request](const std::unique_ptr<DecodeRequest>& ptr) {
                           return ptr.get() == request;
                         });
  CHECK(it != decode_requests_.end());
  decode_requests_.erase(it);
}
