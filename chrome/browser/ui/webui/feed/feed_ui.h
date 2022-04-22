// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_H_

#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace feed {

// TODO(jianli): implement untrusted version of MojoBubbleWebUIController.
class FeedUI : public ui::MojoBubbleWebUIController {
 public:
  explicit FeedUI(content::WebUI* web_ui);

  FeedUI(const FeedUI&) = delete;
  FeedUI& operator=(const FeedUI&) = delete;
  ~FeedUI() override = default;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace feed

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_H_
