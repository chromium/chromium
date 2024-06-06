// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"

// Controller responsible for hosting the data sharing bubble per browser.
class DataSharingBubbleController
    : public BrowserUserData<DataSharingBubbleController> {
 public:
  DataSharingBubbleController(const DataSharingBubbleController&) = delete;
  DataSharingBubbleController& operator=(const DataSharingBubbleController&) =
      delete;
  ~DataSharingBubbleController() override;

  // Shows an instance of the data sharing bubble for this browser.
  void Show();
  // Closes the instance of the data sharing bubble.
  void Close();

 private:
  friend class BrowserUserData<DataSharingBubbleController>;

  explicit DataSharingBubbleController(Browser* browser);

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_
