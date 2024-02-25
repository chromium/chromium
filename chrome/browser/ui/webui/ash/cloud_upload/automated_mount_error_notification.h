// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_AUTOMATED_MOUNT_ERROR_NOTIFICATION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_AUTOMATED_MOUNT_ERROR_NOTIFICATION_H_

class Profile;

namespace ash::cloud_upload {

// Shows the error state for the automated mount indefinitely, until closed by
// the user.
void ShowAutomatedMountErrorNotification(Profile& profile);

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_AUTOMATED_MOUNT_ERROR_NOTIFICATION_H_
