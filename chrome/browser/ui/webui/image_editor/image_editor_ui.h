// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

class Profile;

// This UI and the chrome://image-editor page acts as a wrapper, using an
// <iframe> to display an app hosted from chrome-untrusted://image-editor. The
// mojo interface to handle the user-generated screenshot content will exist
// on the chrome-untrusted page. Note the actual editor app and library is
// reviewed and controlled by us.
class ImageEditorUI : public content::WebUIController {
 public:
  explicit ImageEditorUI(content::WebUI* web_ui);
  ImageEditorUI(const ImageEditorUI&) = delete;
  ImageEditorUI& operator=(const ImageEditorUI&) = delete;
  ~ImageEditorUI() override = default;

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UI_H_
