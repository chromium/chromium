// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_SUB_MENU_MODEL_H_

#include "content/public/browser/web_contents.h"
#include "ui/base/models/simple_menu_model.h"

namespace sharing_hub {

class SharingHubSubMenuModel : public ui::SimpleMenuModel {
 public:
  SharingHubSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                         content::WebContents* web_contents);

 private:
  void Build(content::WebContents* web_contents);

  DISALLOW_COPY_AND_ASSIGN(SharingHubSubMenuModel);
};
}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_SUB_MENU_MODEL_H_
