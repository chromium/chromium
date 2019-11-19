// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "net/base/data_url.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {
namespace settings {

namespace {

// Returns info about extensions for files we support as user images.
ui::SelectFileDialog::FileTypeInfo GetUserImageFileTypeInfo() {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("bmp"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("png"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("tif"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("tiff"));

  file_type_info.extension_description_overrides.resize(1);
  file_type_info.extension_description_overrides[0] =
      l10n_util::GetStringUTF16(IDS_IMAGE_FILES);

  return file_type_info;
}

// Time histogram suffix for profile image download.
const char kProfileDownloadReason[] = "Preferences";

}  // namespace

ChangePictureHandler::ChangePictureHandler()
    : previous_image_index_(user_manager::User::USER_IMAGE_INVALID) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  audio::SoundsManager* manager = audio::SoundsManager::Get();
  manager->Initialize(SOUND_OBJECT_DELETE,
                      bundle.GetRawDataResource(IDR_SOUND_OBJECT_DELETE_WAV));
  manager->Initialize(SOUND_CAMERA_SNAP,
                      bundle.GetRawDataResource(IDR_SOUND_CAMERA_SNAP_WAV));
}

ChangePictureHandler::~ChangePictureHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ChangePictureHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "chooseFile", base::BindRepeating(&ChangePictureHandler::HandleChooseFile,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "photoTaken", base::BindRepeating(&ChangePictureHandler::HandlePhotoTaken,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "discardPhoto",
      base::BindRepeating(&ChangePictureHandler::HandleDiscardPhoto,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onChangePicturePageInitialized",
      base::BindRepeating(&ChangePictureHandler::HandlePageInitialized,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "selectImage",
      base::BindRepeating(&ChangePictureHandler::HandleSelectImage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestSelectedImage",
      base::BindRepeating(&ChangePictureHandler::HandleRequestSelectedImage,
                          base::Unretained(this)));
}

void ChangePictureHandler::OnJavascriptAllowed() {
  user_manager_observer_.Add(user_manager::UserManager::Get());
  camera_observer_.Add(CameraPresenceNotifier::GetInstance());
}

void ChangePictureHandler::OnJavascriptDisallowed() {
  user_manager_observer_.Remove(user_manager::UserManager::Get());
  camera_observer_.Remove(CameraPresenceNotifier::GetInstance());
}

void ChangePictureHandler::SendDefaultImages() {
  base::DictionaryValue result;
  result.SetInteger("first", default_user_image::GetFirstDefaultImage());
  std::unique_ptr<base::ListValue> default_images =
      default_user_image::GetAsDictionary(true /* all */);
  result.Set("images", std::move(default_images));
  FireWebUIListener("default-images-changed", result);
}

void ChangePictureHandler::HandleChooseFile(const base::ListValue* args) {
  DCHECK(args && args->empty());
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));

  base::FilePath downloads_path;
  if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path)) {
    NOTREACHED();
    return;
  }

  // Static so we initialize it only once.
  static base::NoDestructor<ui::SelectFileDialog::FileTypeInfo> file_type_info(
      GetUserImageFileTypeInfo());

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE), downloads_path,
      file_type_info.get(), 0, FILE_PATH_LITERAL(""), GetBrowserWindow(), NULL);
}

void ChangePictureHandler::HandleDiscardPhoto(const base::ListValue* args) {
  DCHECK(args->empty());
  AccessibilityManager::Get()->PlayEarcon(
      SOUND_OBJECT_DELETE, PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED);
}

void ChangePictureHandler::HandlePhotoTaken(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AccessibilityManager::Get()->PlayEarcon(
      SOUND_CAMERA_SNAP, PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED);

  std::string image_url;
  if (!args || args->GetSize() != 1 || !args->GetString(0, &image_url))
    NOTREACHED();
  DCHECK(!image_url.empty());

  std::string raw_data;
  base::StringPiece url(image_url);
  const char kDataUrlPrefix[] = "data:image/png;base64,";
  const size_t kDataUrlPrefixLength = base::size(kDataUrlPrefix) - 1;
  if (!url.starts_with(kDataUrlPrefix) ||
      !base::Base64Decode(url.substr(kDataUrlPrefixLength), &raw_data)) {
    LOG(WARNING) << "Invalid image URL";
    return;
  }

  // Use |raw_data| as image but first verify that it can be decoded.
  user_photo_ = gfx::ImageSkia();
  std::vector<unsigned char> photo_data(raw_data.begin(), raw_data.end());
  user_photo_data_ = base::RefCountedBytes::TakeVector(&photo_data);

  ImageDecoder::Cancel(this);
  ImageDecoder::Start(this, raw_data);
}

void ChangePictureHandler::HandlePageInitialized(const base::ListValue* args) {
  DCHECK(args && args->empty());

  AllowJavascript();

  SendDefaultImages();
  SendSelectedImage();
  UpdateProfileImage();
}

void ChangePictureHandler::SendSelectedImage() {
  const user_manager::User* user = GetUser();
  DCHECK(user->GetAccountId().is_valid());

  previous_image_index_ = user->image_index();
  switch (previous_image_index_) {
    case user_manager::User::USER_IMAGE_EXTERNAL: {
      // User has image from camera/file, record it and add to the image list.
      previous_image_ = user->GetImage();
      previous_image_format_ = user->image_format();
      if (previous_image_format_ == user_manager::UserImage::FORMAT_PNG &&
          user->has_image_bytes()) {
        previous_image_bytes_ = user->image_bytes();
        SendOldImage(webui::GetPngDataUrl(previous_image_bytes_->front(),
                                          previous_image_bytes_->size()));
      } else {
        previous_image_bytes_ = nullptr;
        DCHECK(previous_image_.IsThreadSafe());
        // Post a task because GetBitmapDataUrl does PNG encoding, which is
        // slow for large images.
        base::PostTaskAndReplyWithResult(
            FROM_HERE, {base::ThreadPool(), base::TaskPriority::USER_BLOCKING},
            base::BindOnce(&webui::GetBitmapDataUrl, *previous_image_.bitmap()),
            base::BindOnce(&ChangePictureHandler::SendOldImage,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    }
    case user_manager::User::USER_IMAGE_PROFILE: {
      // User has their Profile image as the current image.
      SendProfileImage(user->GetImage(), true);
      break;
    }
    default: {
      if (default_user_image::IsInCurrentImageSet(previous_image_index_)) {
        // User has image from the current set of default images.
        base::Value image_url(
            default_user_image::GetDefaultImageUrl(previous_image_index_));
        FireWebUIListener("selected-image-changed", image_url);
      } else {
        // User has an old default image, so present it in the same manner as a
        // previous image from file.
        previous_image_ = user->GetImage();
        previous_image_bytes_ = nullptr;
        previous_image_format_ = user_manager::UserImage::FORMAT_UNKNOWN;
        SendOldImageWithIndex(
            default_user_image::GetDefaultImageUrl(previous_image_index_),
            previous_image_index_);
      }
    }
  }
}

void ChangePictureHandler::SendProfileImage(const gfx::ImageSkia& image,
                                            bool should_select) {
  base::Value data_url(webui::GetBitmapDataUrl(*image.bitmap()));
  base::Value select(should_select);
  FireWebUIListener("profile-image-changed", data_url, select);
}

void ChangePictureHandler::UpdateProfileImage() {
  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(GetUser()->GetAccountId());
  // If we have a downloaded profile image and haven't sent it in
  // |SendSelectedImage|, send it now (without selecting).
  if (previous_image_index_ != user_manager::User::USER_IMAGE_PROFILE &&
      !user_image_manager->DownloadedProfileImage().isNull()) {
    SendProfileImage(user_image_manager->DownloadedProfileImage(), false);
  }
  user_image_manager->DownloadProfileImage(kProfileDownloadReason);
}

void ChangePictureHandler::SendOldImage(std::string&& image_url) {
  SendOldImageWithIndex(std::move(image_url), -1);
}

void ChangePictureHandler::SendOldImageWithIndex(std::string&& image_url,
                                                 int image_index) {
  base::DictionaryValue result;
  result.SetStringPath("url", std::move(image_url));
  result.SetIntPath("index", image_index);
  FireWebUIListener("old-image-changed", result);
}

void ChangePictureHandler::HandleSelectImage(const base::ListValue* args) {
  std::string image_url;
  std::string image_type;
  if (!args || args->GetSize() != 2 || !args->GetString(0, &image_url) ||
      !args->GetString(1, &image_type)) {
    NOTREACHED();
    return;
  }
  // |image_url| may be empty unless |image_type| is "default".
  DCHECK(!image_type.empty());

  UserImageManager* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(GetUser()->GetAccountId());
  bool waiting_for_camera_photo = false;

  if (image_type == "old") {
    // Previous image (from camera or manually uploaded) re-selected.
    DCHECK(!previous_image_.isNull());
    std::unique_ptr<user_manager::UserImage> user_image;
    if (previous_image_format_ == user_manager::UserImage::FORMAT_PNG &&
        previous_image_bytes_) {
      user_image = std::make_unique<user_manager::UserImage>(
          previous_image_, previous_image_bytes_, previous_image_format_);
      user_image->MarkAsSafe();
    } else {
      user_image = user_manager::UserImage::CreateAndEncode(
          previous_image_, user_manager::UserImage::FORMAT_JPEG);
    }
    user_image_manager->SaveUserImage(std::move(user_image));

    UMA_HISTOGRAM_EXACT_LINEAR("UserImage.ChangeChoice",
                               default_user_image::kHistogramImageOld,
                               default_user_image::kHistogramImagesCount);
    VLOG(1) << "Selected old user image";
  } else if (image_type == "default") {
    int image_index = user_manager::User::USER_IMAGE_INVALID;
    if (default_user_image::IsDefaultImageUrl(image_url, &image_index)) {
      // One of the default user images.
      user_image_manager->SaveUserDefaultImageIndex(image_index);

      UMA_HISTOGRAM_EXACT_LINEAR(
          "UserImage.ChangeChoice",
          default_user_image::GetDefaultImageHistogramValue(image_index),
          default_user_image::kHistogramImagesCount);
      VLOG(1) << "Selected default user image: " << image_index;
    } else {
      LOG(WARNING) << "Invalid image_url for default image type: " << image_url;
    }
  } else if (image_type == "camera") {
    // Camera image is selected.
    if (user_photo_.isNull()) {
      waiting_for_camera_photo = true;
      VLOG(1) << "Still waiting for camera image to decode";
    } else {
      SetImageFromCamera(user_photo_, user_photo_data_.get());
    }
  } else if (image_type == "profile") {
    // Profile image selected. Could be previous (old) user image.
    user_image_manager->SaveUserImageFromProfileImage();

    if (previous_image_index_ == user_manager::User::USER_IMAGE_PROFILE) {
      UMA_HISTOGRAM_EXACT_LINEAR("UserImage.ChangeChoice",
                                 default_user_image::kHistogramImageOld,
                                 default_user_image::kHistogramImagesCount);
      VLOG(1) << "Selected old (profile) user image";
    } else {
      UMA_HISTOGRAM_EXACT_LINEAR("UserImage.ChangeChoice",
                                 default_user_image::kHistogramImageFromProfile,
                                 default_user_image::kHistogramImagesCount);
      VLOG(1) << "Selected profile image";
    }
  } else {
    NOTREACHED() << "Unexpected image type: " << image_type;
  }

  // Ignore the result of the previous decoding if it's no longer needed.
  if (!waiting_for_camera_photo)
    ImageDecoder::Cancel(this);
}

void ChangePictureHandler::HandleRequestSelectedImage(
    const base::ListValue* args) {
  SendSelectedImage();
}

void ChangePictureHandler::FileSelected(const base::FilePath& path,
                                        int index,
                                        void* params) {
  ChromeUserManager::Get()
      ->GetUserImageManager(GetUser()->GetAccountId())
      ->SaveUserImageFromFile(path);
  UMA_HISTOGRAM_EXACT_LINEAR("UserImage.ChangeChoice",
                             default_user_image::kHistogramImageFromFile,
                             default_user_image::kHistogramImagesCount);
  VLOG(1) << "Selected image from file";
}

void ChangePictureHandler::SetImageFromCamera(
    const gfx::ImageSkia& photo,
    base::RefCountedBytes* photo_bytes) {
  std::unique_ptr<user_manager::UserImage> user_image =
      std::make_unique<user_manager::UserImage>(
          photo, photo_bytes, user_manager::UserImage::FORMAT_PNG);
  user_image->MarkAsSafe();
  ChromeUserManager::Get()
      ->GetUserImageManager(GetUser()->GetAccountId())
      ->SaveUserImage(std::move(user_image));
  UMA_HISTOGRAM_EXACT_LINEAR("UserImage.ChangeChoice",
                             default_user_image::kHistogramImageFromCamera,
                             default_user_image::kHistogramImagesCount);
  VLOG(1) << "Selected camera photo";
}

void ChangePictureHandler::SetCameraPresent(bool present) {
  FireWebUIListener("camera-presence-changed", base::Value(present));
}

void ChangePictureHandler::OnCameraPresenceCheckDone(bool is_camera_present) {
  SetCameraPresent(is_camera_present);
}

void ChangePictureHandler::OnUserImageChanged(const user_manager::User& user) {
  // Not initialized yet.
  if (previous_image_index_ == user_manager::User::USER_IMAGE_INVALID)
    return;
  SendSelectedImage();
}

void ChangePictureHandler::OnUserProfileImageUpdated(
    const user_manager::User& user,
    const gfx::ImageSkia& profile_image) {
  // User profile image has been updated.
  SendProfileImage(profile_image, false);
}

gfx::NativeWindow ChangePictureHandler::GetBrowserWindow() const {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  return browser->window()->GetNativeWindow();
}

void ChangePictureHandler::OnImageDecoded(const SkBitmap& decoded_image) {
  user_photo_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
  SetImageFromCamera(user_photo_, user_photo_data_.get());
}

void ChangePictureHandler::OnDecodeImageFailed() {
  NOTREACHED() << "Failed to decode PNG image from WebUI";
}

const user_manager::User* ChangePictureHandler::GetUser() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return user_manager::UserManager::Get()->GetActiveUser();
  return user;
}

}  // namespace settings
}  // namespace chromeos
