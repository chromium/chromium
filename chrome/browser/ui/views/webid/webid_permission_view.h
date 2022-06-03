// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_VIEW_H_

#include <memory>
#include "chrome/browser/ui/views/webid/webid_dialog_views.h"
#include "ui/views/view.h"

// Basic permission dialog that is used to ask for user approval at different
// points in the WebID flow.
//
// It shows a message with two buttons to cancel/accept e.g.,
// "Would you like to do X?  [Cancel]  [Continue]
class WebIdPermissionView : public views::View {
 public:
  static std::unique_ptr<WebIdPermissionView> CreateForInitialPermission(
      WebIdDialogViews* dialog,
      const std::u16string& idp_hostname,
      const std::u16string& rp_hostname);
  static std::unique_ptr<WebIdPermissionView> CreateForTokenExchangePermission(
      WebIdDialogViews* dialog,
      const std::u16string& idp_hostname,
      const std::u16string& rp_hostname);

  WebIdPermissionView(WebIdDialogViews* dialog,
                      std::unique_ptr<views::View> content);
  WebIdPermissionView(const WebIdPermissionView&) = delete;
  WebIdPermissionView operator=(const WebIdPermissionView&) = delete;
  ~WebIdPermissionView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_VIEW_H_
