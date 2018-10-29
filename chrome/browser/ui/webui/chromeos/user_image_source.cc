// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/user_image_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#include "chrome/common/url_constants.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/third_party/mozilla/url_parse.h"

namespace {

// URL parameter specifying frame index.
const char kFrameIndex[] = "frame";

// Parses the user image URL, which looks like
// "chrome://userimage/serialized-user-id?key1=value1&...&key_n=value_n",
// to user email and frame.
void ParseRequest(const GURL& url, std::string* email, int* frame) {
  DCHECK(url.is_valid());
  const std::string serialized_account_id = net::UnescapeURLComponent(
      url.path().substr(1),
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
          net::UnescapeRule::PATH_SEPARATORS | net::UnescapeRule::SPACES);
  AccountId account_id(EmptyAccountId());
  const bool status =
      AccountId::Deserialize(serialized_account_id, &account_id);
  // TODO(alemate): DCHECK(status) - should happen after options page is
  // migrated.
  if (!status) {
    LOG(WARNING) << "Failed to deserialize account_id.";
    account_id = user_manager::known_user::GetAccountId(
        serialized_account_id, std::string() /* id */, AccountType::UNKNOWN);
  }
  *email = account_id.GetUserEmail();
  *frame = -1;
  base::StringPairs parameters;
  base::SplitStringIntoKeyValuePairs(url.query(), '=', '&', &parameters);
  for (base::StringPairs::const_iterator iter = parameters.begin();
       iter != parameters.end(); ++iter) {
    if (iter->first == kFrameIndex) {
      unsigned value = 0;
      if (!base::StringToUint(iter->second, &value)) {
        LOG(WARNING) << "Invalid frame format: " << iter->second;
        continue;
      }
      *frame = static_cast<int>(value);
      break;
    }
  }
}

scoped_refptr<base::RefCountedMemory> LoadUserImageFrameForScaleFactor(
    int resource_id,
    int frame,
    ui::ScaleFactor scale_factor) {
  // Load all frames.
  if (frame == -1) {
    return ui::ResourceBundle::GetSharedInstance()
        .LoadDataResourceBytesForScale(resource_id, scale_factor);
  }
  // TODO(reveman): Add support for frames beyond 0 (crbug.com/750064).
  if (frame) {
    NOTIMPLEMENTED() << "Unsupported frame: " << frame;
    return nullptr;
  }
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  float scale = ui::GetScaleForScaleFactor(scale_factor);
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes);
  gfx::PNGCodec::EncodeBGRASkBitmap(image->GetRepresentation(scale).GetBitmap(),
                                    false /* discard transparency */,
                                    &data->data());
  return data;
}

scoped_refptr<base::RefCountedMemory> GetUserImageFrame(
    scoped_refptr<base::RefCountedMemory> image_bytes,
    user_manager::UserImage::ImageFormat image_format,
    int frame) {
  // Return all frames.
  if (frame == -1)
    return image_bytes;
  // TODO(reveman): Add support for frames beyond 0 (crbug.com/750064).
  if (frame) {
    NOTIMPLEMENTED() << "Unsupported frame: " << frame;
    return nullptr;
  }
  // Only PNGs can be animated.
  if (image_format != user_manager::UserImage::FORMAT_PNG)
    return image_bytes;
  // Extract first frame by re-encoding image.
  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(image_bytes->front(), image_bytes->size(),
                             &bitmap)) {
    return nullptr;
  }
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false /* discard transparency */,
                                    &data->data());
  return data;
}

scoped_refptr<base::RefCountedMemory> GetUserImageInternal(
    const AccountId& account_id,
    int frame) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_100P;
  // Use the scaling that matches primary display. These source images are
  // 96x96 and often used at that size in WebUI pages.
  display::Screen* screen = display::Screen::GetScreen();
  if (screen) {
    scale_factor = ui::GetSupportedScaleFactor(
        screen->GetPrimaryDisplay().device_scale_factor());
  }

  if (user) {
    if (user->has_image_bytes()) {
      if (user->image_format() == user_manager::UserImage::FORMAT_PNG) {
        return GetUserImageFrame(user->image_bytes(), user->image_format(),
                                 frame);
      } else {
        scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes);
        gfx::PNGCodec::EncodeBGRASkBitmap(*user->GetImage().bitmap(),
                                          false /* discard transparency */,
                                          &data->data());
        return data;
      }
    }
    if (user->image_is_stub()) {
      return LoadUserImageFrameForScaleFactor(IDR_LOGIN_DEFAULT_USER, frame,
                                              scale_factor);
    }
    if (user->HasDefaultImage()) {
      return LoadUserImageFrameForScaleFactor(
          chromeos::default_user_image::kDefaultImageResourceIDs
              [user->image_index()],
          frame, scale_factor);
    }
    NOTREACHED() << "User with custom image missing data bytes";
  } else {
    LOG(ERROR) << "User not found: " << account_id.GetUserEmail();
  }
  return LoadUserImageFrameForScaleFactor(IDR_LOGIN_DEFAULT_USER, frame,
                                          scale_factor);
}

}  // namespace

namespace chromeos {

// Static.
scoped_refptr<base::RefCountedMemory> UserImageSource::GetUserImage(
    const AccountId& account_id) {
  return GetUserImageInternal(account_id, -1);
}

UserImageSource::UserImageSource() {}

UserImageSource::~UserImageSource() {}

std::string UserImageSource::GetSource() const {
  return chrome::kChromeUIUserImageHost;
}

void UserImageSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string email;
  int frame = -1;
  GURL url(chrome::kChromeUIUserImageURL + path);
  ParseRequest(url, &email, &frame);
  const AccountId account_id(AccountId::FromUserEmail(email));
  callback.Run(GetUserImageInternal(account_id, frame));
}

std::string UserImageSource::GetMimeType(const std::string& path) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

}  // namespace chromeos
