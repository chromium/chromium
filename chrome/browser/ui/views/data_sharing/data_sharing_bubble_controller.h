// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
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

  // `request_info` contains the values we want to pass into the loaded WebUI in
  // this bubble.
  void Show(std::variant<tab_groups::LocalTabGroupID, data_sharing::GroupToken>
                request_info);
  // Closes the instance of the data sharing bubble.
  void Close();

 private:
  friend class BrowserUserData<DataSharingBubbleController>;

  explicit DataSharingBubbleController(Browser* browser);

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_DATA_SHARING_DATA_SHARING_BUBBLE_CONTROLLER_H_
