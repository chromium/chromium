// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class ListValue;
}

namespace user_manager {
class User;
}

namespace chromeos {

namespace settings {

// ChromeOS user image settings page UI handler.
class ChangePictureHandler : public ::settings::SettingsPageUIHandler,
                             public ui::SelectFileDialog::Listener,
                             public user_manager::UserManager::Observer,
                             public ImageDecoder::ImageRequest,
                             public CameraPresenceNotifier::Observer {
 public:
  ChangePictureHandler();
  ~ChangePictureHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // CameraPresenceNotifier::Observer implementation:
  void OnCameraPresenceCheckDone(bool is_camera_present) override;

 private:
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

  // Sends the previous user image to the page.
  void SendOldImage(std::string&& image_url);

  // Sends the previous user image to the page. Also sends |image_index| which
  // is either the index of the previous user image (if it was from an older
  // default image set) or -1 otherwise. This allows the WebUI to show credits
  // for older default images.
  void SendOldImageWithIndex(std::string&& image_url, int image_index);

  // Starts camera presence check.
  void CheckCameraPresence();

  // Updates UI with camera presence state.
  void SetCameraPresent(bool present);

  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::ListValue* args);

  // Handles photo taken with WebRTC UI.
  void HandlePhotoTaken(const base::ListValue* args);

  // Handles 'discard-photo' button click.
  void HandleDiscardPhoto(const base::ListValue* args);

  // Gets the list of available user images and sends it to the page.
  void HandleGetAvailableImages(const base::ListValue* args);

  // Handles page initialized event.
  void HandlePageInitialized(const base::ListValue* args);

  // Selects one of the available images as user's.
  void HandleSelectImage(const base::ListValue* args);

  // Requests the currently selected image.
  void HandleRequestSelectedImage(const base::ListValue* args);

  // SelectFileDialog::Delegate implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  // user_manager::UserManager::Observer implementation.
  void OnUserImageChanged(const user_manager::User& user) override;
  void OnUserProfileImageUpdated(const user_manager::User& user,
                                 const gfx::ImageSkia& profile_image) override;

  // Sets user image to photo taken from camera.
  void SetImageFromCamera(const gfx::ImageSkia& photo,
                          base::RefCountedBytes* image_bytes);

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  // Overriden from ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  // Returns user related to current WebUI. If this user doesn't exist,
  // returns active user.
  const user_manager::User* GetUser() const;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

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

  ScopedObserver<user_manager::UserManager, user_manager::UserManager::Observer>
      user_manager_observer_{this};
  ScopedObserver<CameraPresenceNotifier, CameraPresenceNotifier::Observer>
      camera_observer_{this};

  base::WeakPtrFactory<ChangePictureHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChangePictureHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_
