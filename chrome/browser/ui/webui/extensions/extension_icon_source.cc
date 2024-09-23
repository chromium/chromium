// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"

#include <stddef.h>

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/component_extension_resources_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "url/gurl.h"

namespace extensions {

namespace {

scoped_refptr<base::RefCountedMemory> BitmapToMemory(const SkBitmap* image) {
  auto image_bytes = base::MakeRefCounted<base::RefCountedBytes>();
  gfx::PNGCodec::EncodeBGRASkBitmap(*image, false, &image_bytes->as_vector());
  return image_bytes;
}

SkBitmap DesaturateImage(const SkBitmap* image) {
  color_utils::HSL shift = {-1, 0, 0.6};
  return SkBitmapOperations::CreateHSLShiftedBitmap(*image, shift);
}

SkBitmap* ToBitmap(const unsigned char* data, size_t size) {
  SkBitmap* decoded = new SkBitmap();
  bool success = gfx::PNGCodec::Decode(data, size, decoded);
  DCHECK(success);
  return decoded;
}

}  // namespace

ExtensionIconSource::ExtensionIconSource(Profile* profile) : profile_(profile) {
}

struct ExtensionIconSource::ExtensionIconRequest {
  content::URLDataSource::GotDataCallback callback;
  scoped_refptr<const Extension> extension;
  bool grayscale;
  int size;
  ExtensionIconSet::Match match;
};

// static
GURL ExtensionIconSource::GetIconURL(const Extension* extension,
                                     int icon_size,
                                     ExtensionIconSet::Match match,
                                     bool grayscale) {
  return GetIconURL(extension->id(), icon_size, match, grayscale);
}

// static
GURL ExtensionIconSource::GetIconURL(const std::string& extension_id,
                                     int icon_size,
                                     ExtensionIconSet::Match match,
                                     bool grayscale) {
  GURL icon_url(base::StringPrintf(
      "%s%s/%d/%d%s", chrome::kChromeUIExtensionIconURL, extension_id.c_str(),
      icon_size, static_cast<int>(match), grayscale ? "?grayscale=true" : ""));
  CHECK(icon_url.is_valid());
  return icon_url;
}

// static
SkBitmap* ExtensionIconSource::LoadImageByResourceId(int resource_id) {
  std::string_view contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          resource_id, ui::k100Percent);

  // Convert and return it.
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(contents.data());
  return ToBitmap(data, contents.length());
}

std::string ExtensionIconSource::GetSource() {
  return chrome::kChromeUIExtensionIconHost;
}

std::string ExtensionIconSource::GetMimeType(const GURL&) {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

void ExtensionIconSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  // This is where everything gets started. First, parse the request and make
  // the request data available for later.
  static int next_id = 0;
  if (!ParseData(path, ++next_id, &callback)) {
    // If the request data cannot be parsed, request parameters will not be
    // added to |request_map_|.
    // Send back the default application icon (not resized or desaturated) as
    // the default response.
    std::move(callback).Run(BitmapToMemory(GetDefaultAppImage()).get());
    return;
  }

  ExtensionIconRequest* request = GetData(next_id);
  ExtensionResource icon = IconsInfo::GetIconResource(
      request->extension.get(), request->size, request->match);

  if (icon.relative_path().empty()) {
    LoadIconFailed(next_id);
  } else {
    LoadExtensionImage(icon, next_id);
  }
}

bool ExtensionIconSource::AllowCaching() {
  // Should not be cached to reflect the latest contents that may be updated by
  // Extensions.
  return false;
}

ExtensionIconSource::~ExtensionIconSource() {
}

const SkBitmap* ExtensionIconSource::GetDefaultAppImage() {
  if (!default_app_data_.get())
    default_app_data_.reset(LoadImageByResourceId(IDR_APP_DEFAULT_ICON));

  return default_app_data_.get();
}

const SkBitmap* ExtensionIconSource::GetDefaultExtensionImage() {
  if (!default_extension_data_.get()) {
    default_extension_data_.reset(
        LoadImageByResourceId(IDR_EXTENSION_DEFAULT_ICON));
  }

  return default_extension_data_.get();
}

void ExtensionIconSource::FinalizeImage(const SkBitmap* image,
                                        int request_id) {
  SkBitmap bitmap;
  ExtensionIconRequest* request = GetData(request_id);
  if (request->grayscale)
    bitmap = DesaturateImage(image);
  else
    bitmap = *image;

  std::move(request->callback).Run(BitmapToMemory(&bitmap).get());
  ClearData(request_id);
}

void ExtensionIconSource::LoadDefaultImage(int request_id) {
  ExtensionIconRequest* request = GetData(request_id);
  const SkBitmap* default_image = nullptr;

  if (request->extension->is_app())
    default_image = GetDefaultAppImage();
  else
    default_image = GetDefaultExtensionImage();

  SkBitmap resized_image(skia::ImageOperations::Resize(
      *default_image, skia::ImageOperations::RESIZE_LANCZOS3,
      request->size, request->size));

  // There are cases where Resize returns an empty bitmap, for example if you
  // ask for an image too large. In this case it is better to return the default
  // image than returning nothing at all.
  if (resized_image.empty())
    resized_image = *default_image;

  FinalizeImage(&resized_image, request_id);
}

void ExtensionIconSource::LoadExtensionImage(const ExtensionResource& icon,
                                             int request_id) {
  ExtensionIconRequest* request = GetData(request_id);
  ImageLoader::Get(profile_)->LoadImageAsync(
      request->extension.get(), icon, gfx::Size(request->size, request->size),
      base::BindOnce(&ExtensionIconSource::OnImageLoaded,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void ExtensionIconSource::LoadFaviconImage(int request_id) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  // Fall back to the default icons if the service isn't available.
  if (favicon_service == nullptr) {
    LoadDefaultImage(request_id);
    return;
  }

  GURL favicon_url =
      AppLaunchInfo::GetFullLaunchURL(GetData(request_id)->extension.get());
  favicon_service->GetRawFaviconForPageURL(
      favicon_url, {favicon_base::IconType::kFavicon}, gfx::kFaviconSize,
      /*fallback_to_host=*/false,
      base::BindOnce(&ExtensionIconSource::OnFaviconDataAvailable,
                     base::Unretained(this), request_id),
      &cancelable_task_tracker_);
}

void ExtensionIconSource::OnFaviconDataAvailable(
    int request_id,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  ExtensionIconRequest* request = GetData(request_id);

  // Fallback to the default icon if there wasn't a favicon.
  if (!bitmap_result.is_valid()) {
    LoadDefaultImage(request_id);
    return;
  }

  if (!request->grayscale) {
    // If we don't need a grayscale image, then we can bypass FinalizeImage
    // to avoid unnecessary conversions.
    std::move(request->callback).Run(bitmap_result.bitmap_data.get());
    ClearData(request_id);
  } else {
    FinalizeImage(ToBitmap(bitmap_result.bitmap_data->data(),
                           bitmap_result.bitmap_data->size()),
                  request_id);
  }
}

void ExtensionIconSource::OnImageLoaded(int request_id,
                                        const gfx::Image& image) {
  if (image.IsEmpty())
    LoadIconFailed(request_id);
  else
    FinalizeImage(image.ToSkBitmap(), request_id);
}

void ExtensionIconSource::LoadIconFailed(int request_id) {
  ExtensionIconRequest* request = GetData(request_id);
  ExtensionResource icon = IconsInfo::GetIconResource(
      request->extension.get(), request->size, request->match);

  if (request->size == extension_misc::EXTENSION_ICON_BITTY)
    LoadFaviconImage(request_id);
  else
    LoadDefaultImage(request_id);
}

bool ExtensionIconSource::ParseData(
    const std::string& path,
    int request_id,
    content::URLDataSource::GotDataCallback* callback) {
  // Extract the parameters from the path by lower casing and splitting.
  std::string path_lower = base::ToLowerASCII(path);
  std::vector<std::string> path_parts = base::SplitString(
      path_lower, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (path_lower.empty() || path_parts.size() < 3)
    return false;

  std::string size_param = path_parts.at(1);
  std::string match_param = path_parts.at(2);
  match_param = match_param.substr(0, match_param.find('?'));

  int size;
  if (!base::StringToInt(size_param, &size))
    return false;
  if (size <= 0 || size > extension_misc::EXTENSION_ICON_GIGANTOR)
    return false;

  ExtensionIconSet::Match match_type;
  int match_num;
  if (!base::StringToInt(match_param, &match_num))
    return false;
  match_type = static_cast<ExtensionIconSet::Match>(match_num);
  if (!(match_type == ExtensionIconSet::Match::kExactly ||
        match_type == ExtensionIconSet::Match::kSmaller ||
        match_type == ExtensionIconSet::Match::kBigger)) {
    match_type = ExtensionIconSet::Match::kExactly;
  }

  std::string extension_id = path_parts.at(0);
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetInstalledExtension(extension_id);
  if (!extension)
    return false;

  bool grayscale = path_lower.find("grayscale=true") != std::string::npos;

  SetData(request_id, std::move(*callback), extension, grayscale, size,
          match_type);

  return true;
}

void ExtensionIconSource::SetData(
    int request_id,
    content::URLDataSource::GotDataCallback callback,
    const Extension* extension,
    bool grayscale,
    int size,
    ExtensionIconSet::Match match) {
  std::unique_ptr<ExtensionIconRequest> request =
      std::make_unique<ExtensionIconRequest>();
  request->callback = std::move(callback);
  request->extension = extension;
  request->grayscale = grayscale;
  request->size = size;
  request->match = match;
  request_map_[request_id] = std::move(request);
}

ExtensionIconSource::ExtensionIconRequest* ExtensionIconSource::GetData(
    int request_id) {
  return request_map_[request_id].get();
}

void ExtensionIconSource::ClearData(int request_id) {
  request_map_.erase(request_id);
}

}  // namespace extensions
