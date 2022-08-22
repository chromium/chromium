// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/camera_presence_notifier.h"
#include "chrome/browser/ash/login/users/avatar/user_image_file_selector.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/image/image_skia.h"

namespace user_manager {
class User;
}

namespace chromeos {

namespace settings {

// ChromeOS user image settings page UI handler.
class ChangePictureHandler : public ::settings::SettingsPageUIHandler,
                             public user_manager::UserManager::Observer,
                             public ImageDecoder::ImageRequest {
 public:
  ChangePictureHandler();

  ChangePictureHandler(const ChangePictureHandler&) = delete;
  ChangePictureHandler& operator=(const ChangePictureHandler&) = delete;

  ~ChangePictureHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class ChangePictureHandlerTest;

  // Sends list of available default images to the page.
  void SendDefaultImages();

  // Sends current selection to the page.
  void SendSelectedImage();

  // Sends the profile image to the page. If |should_select| is true then
  // the profile image element is selected.
  void SendProfileImage(const gfx::ImageSkia& image, bool should_select);

  // Starts profile image update and shows the last downloaded profile image,
  // if any, on the page. Shouldn't be called before |SendProfileImage|.
  void UpdateProfileImage();

  // Sends the previous user image from camera or file to the page.
  void SendOldImage(std::string&& image_url);

  // Updates UI with camera presence state.
  void SetCameraPresent(bool present);

  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::Value::List& args);

  // Handles photo taken with WebRTC UI.
  void HandlePhotoTaken(const base::Value::List& args);

  // Handles 'discard-photo' button click.
  void HandleDiscardPhoto(const base::Value::List& args);

  // Gets the list of available user images and sends it to the page.
  void HandleGetAvailableImages(const base::Value::List& args);

  // Handles page initialized event.
  void HandlePageInitialized(const base::Value::List& args);

  // Selects one of the available images as user's.
  void HandleSelectImage(const base::Value::List& args);

  // Requests the currently selected image.
  void HandleRequestSelectedImage(const base::Value::List& args);

  void FileSelected(const base::FilePath& path);

  void FileSelectionCanceled();

  // user_manager::UserManager::Observer implementation.
  void OnUserImageChanged(const user_manager::User& user) override;
  void OnUserProfileImageUpdated(const user_manager::User& user,
                                 const gfx::ImageSkia& profile_image) override;

  // Sets user image to photo taken from camera.
  void SetImageFromCamera(const gfx::ImageSkia& photo,
                          base::RefCountedBytes* image_bytes);

  // Overriden from ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  // Returns user related to current WebUI. If this user doesn't exist,
  // returns active user.
  const user_manager::User* GetUser();

  // Previous user image from camera/file and its data URL.
  gfx::ImageSkia previous_image_;
  scoped_refptr<base::RefCountedBytes> previous_image_bytes_;
  user_manager::UserImage::ImageFormat previous_image_format_ =
      user_manager::UserImage::FORMAT_UNKNOWN;

  // Index of the previous user image.
  int previous_image_index_;

  // Last user photo, if taken.
  gfx::ImageSkia user_photo_;

  // Data for |user_photo_|.
  scoped_refptr<base::RefCountedBytes> user_photo_data_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};

  ash::CameraPresenceNotifier camera_presence_notifier_;

  std::unique_ptr<ash::UserImageFileSelector> user_image_file_selector_;

  base::WeakPtrFactory<ChangePictureHandler> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_
