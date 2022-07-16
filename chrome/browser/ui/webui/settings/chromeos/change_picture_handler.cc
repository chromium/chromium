// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"

#include <memory>
#include <utility>

#include "ash/components/audio/sounds.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/camera_presence_notifier.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
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

namespace chromeos {
namespace settings {
namespace {

using ::ash::AccessibilityManager;
using ::ash::PlaySoundOption;
using ::content::BrowserThread;

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

void RecordUserImageChanged(int sample) {
  // Although |ChangePictureHandler::kUserImageChangedHistogramName| is an
  // enumerated histogram, we intentionally use UmaHistogramExactLinear() to
  // emit the metric rather than UmaHistogramEnumeration(). This is because the
  // enums.xml values correspond to (a) special constants and (b) indexes of an
  // array containing resource IDs.
  base::UmaHistogramExactLinear(
      ChangePictureHandler::kUserImageChangedHistogramName, sample,
      default_user_image::kHistogramImagesCount + 1);
}

}  // namespace

const char ChangePictureHandler::kUserImageChangedHistogramName[] =
    "UserImage.Changed2";

ChangePictureHandler::ChangePictureHandler()
    : previous_image_index_(user_manager::User::USER_IMAGE_INVALID) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  audio::SoundsManager* manager = audio::SoundsManager::Get();
  manager->Initialize(static_cast<int>(Sound::kObjectDelete),
                      bundle.GetRawDataResource(IDR_SOUND_OBJECT_DELETE_WAV));
  manager->Initialize(static_cast<int>(Sound::kCameraSnap),
                      bundle.GetRawDataResource(IDR_SOUND_CAMERA_SNAP_WAV));
}

ChangePictureHandler::~ChangePictureHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ChangePictureHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "chooseFile", base::BindRepeating(&ChangePictureHandler::HandleChooseFile,
                                        base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "photoTaken", base::BindRepeating(&ChangePictureHandler::HandlePhotoTaken,
                                        base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "discardPhoto",
      base::BindRepeating(&ChangePictureHandler::HandleDiscardPhoto,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "onChangePicturePageInitialized",
      base::BindRepeating(&ChangePictureHandler::HandlePageInitialized,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "selectImage",
      base::BindRepeating(&ChangePictureHandler::HandleSelectImage,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "requestSelectedImage",
      base::BindRepeating(&ChangePictureHandler::HandleRequestSelectedImage,
                          base::Unretained(this)));
}

void ChangePictureHandler::OnJavascriptAllowed() {
  user_manager_observation_.Observe(user_manager::UserManager::Get());
  camera_observation_.Observe(CameraPresenceNotifier::GetInstance());
}

void ChangePictureHandler::OnJavascriptDisallowed() {
  DCHECK(user_manager_observation_.IsObservingSource(
      user_manager::UserManager::Get()));
  user_manager_observation_.Reset();

  DCHECK(camera_observation_.IsObservingSource(
      CameraPresenceNotifier::GetInstance()));
  camera_observation_.Reset();

  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ChangePictureHandler::SendDefaultImages() {
  base::DictionaryValue result;
  std::unique_ptr<base::ListValue> current_default_images =
      default_user_image::GetCurrentImageSet();
  result.SetKey(
      "current_default_images",
      base::Value::FromUniquePtrValue(std::move(current_default_images)));
  FireWebUIListener("default-images-changed", result);
}

void ChangePictureHandler::HandleChooseFile(const base::ListValue* args) {
  DCHECK(args && args->GetList().empty());
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
  DCHECK(args->GetList().empty());
  AccessibilityManager::Get()->PlayEarcon(
      Sound::kObjectDelete, PlaySoundOption::kOnlyIfSpokenFeedbackEnabled);
}

void ChangePictureHandler::HandlePhotoTaken(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AccessibilityManager::Get()->PlayEarcon(
      Sound::kCameraSnap, PlaySoundOption::kOnlyIfSpokenFeedbackEnabled);

  if (!args || args->GetList().size() != 1 || !args->GetList()[0].is_string())
    NOTREACHED();
  const std::string& image_url = args->GetList()[0].GetString();
  DCHECK(!image_url.empty());

  std::string raw_data;
  base::StringPiece url(image_url);
  const char kDataUrlPrefix[] = "data:image/png;base64,";
  const size_t kDataUrlPrefixLength = base::size(kDataUrlPrefix) - 1;
  if (!base::StartsWith(url, kDataUrlPrefix) ||
      !base::Base64Decode(url.substr(kDataUrlPrefixLength), &raw_data)) {
    LOG(WARNING) << "Invalid image URL";
    return;
  }

  // Use |raw_data| as image but first verify that it can be decoded.
  user_photo_ = gfx::ImageSkia();
  std::vector<unsigned char> photo_data(raw_data.begin(), raw_data.end());
  user_photo_data_ = base::RefCountedBytes::TakeVector(&photo_data);

  ImageDecoder::Cancel(this);
  ImageDecoder::Start(this, std::move(raw_data));
}

void ChangePictureHandler::HandlePageInitialized(const base::ListValue* args) {
  DCHECK(args && args->GetList().empty());

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
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::TaskPriority::USER_BLOCKING},
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
        // User has a deprecated default image, send it for preview.
        previous_image_ = user->GetImage();
        previous_image_bytes_ = nullptr;
        previous_image_format_ = user_manager::UserImage::FORMAT_UNKNOWN;

        base::DictionaryValue result;
        result.SetStringPath("url", default_user_image::GetDefaultImageUrl(
                                        previous_image_index_));
        auto source_info = default_user_image::GetDefaultImageSourceInfo(
            previous_image_index_);
        if (source_info.has_value()) {
          result.SetStringPath("author", l10n_util::GetStringUTF16(std::move(
                                             source_info.value().author_id)));
          result.SetStringPath("website", l10n_util::GetStringUTF16(std::move(
                                              source_info.value().website_id)));
        }
        FireWebUIListener("preview-deprecated-image", result);
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
  auto* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(GetUser()->GetAccountId());
  // If we have a downloaded profile image and haven't sent it in
  // |SendSelectedImage|, send it now (without selecting).
  if (previous_image_index_ != user_manager::User::USER_IMAGE_PROFILE &&
      !user_image_manager->DownloadedProfileImage().isNull()) {
    SendProfileImage(user_image_manager->DownloadedProfileImage(), false);
  }
  user_image_manager->DownloadProfileImage();
}

void ChangePictureHandler::SendOldImage(std::string&& image_url) {
  FireWebUIListener("old-image-changed", base::Value(image_url));
}

void ChangePictureHandler::HandleSelectImage(const base::ListValue* args) {
  if (!args || args->GetList().size() != 2 || !args->GetList()[0].is_string() ||
      !args->GetList()[1].is_string()) {
    NOTREACHED();
    return;
  }
  const std::string& image_url = args->GetList()[0].GetString();
  const std::string& image_type = args->GetList()[1].GetString();
  // |image_url| may be empty unless |image_type| is "default".
  DCHECK(!image_type.empty());

  auto* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(GetUser()->GetAccountId());
  bool waiting_for_camera_photo = false;

  // Track the index of previous selected message to be compared with the index
  // of the new image.
  int previous_image_index = GetUser()->image_index();

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

    VLOG(1) << "Selected old user image";
  } else if (image_type == "default") {
    int image_index = user_manager::User::USER_IMAGE_INVALID;
    if (default_user_image::IsDefaultImageUrl(image_url, &image_index)) {
      // One of the default user images.
      user_image_manager->SaveUserDefaultImageIndex(image_index);

      VLOG(1) << "Selected default user image: " << image_index;
    } else {
      LOG(WARNING) << "Invalid image_url for default image type: " << image_url;
    }
  } else if (image_type == "profile") {
    // Profile image selected. Could be previous (old) user image.
    user_image_manager->SaveUserImageFromProfileImage();
  } else {
    NOTREACHED() << "Unexpected image type: " << image_type;
  }

  int image_index = GetUser()->image_index();
  // `previous_image_index` is used instead of `previous_image_index_` as the
  // latter has the same value of `image_index` after new image is selected.
  if (previous_image_index != image_index) {
    RecordUserImageChanged(
        user_image_manager->ImageIndexToHistogramIndex(image_index));
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
  auto* user_image_manager =
      ChromeUserManager::Get()->GetUserImageManager(GetUser()->GetAccountId());

  // Log an impression if image is selected from a file.
  RecordUserImageChanged(user_image_manager->ImageIndexToHistogramIndex(
      user_manager::User::USER_IMAGE_EXTERNAL));
  user_image_manager->SaveUserImageFromFile(path);
  VLOG(1) << "Selected image from file";
}

void ChangePictureHandler::FileSelectionCanceled(void* params) {
  SendSelectedImage();
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

  // Log an impression if image is taken from photo.
  RecordUserImageChanged(default_user_image::kHistogramImageFromCamera);
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

gfx::NativeWindow ChangePictureHandler::GetBrowserWindow() {
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

const user_manager::User* ChangePictureHandler::GetUser() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return user_manager::UserManager::Get()->GetActiveUser();
  return user;
}

}  // namespace settings
}  // namespace chromeos
