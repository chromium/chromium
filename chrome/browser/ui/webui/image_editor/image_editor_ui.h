// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/image_editor/image_editor.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"

class Profile;

namespace image_editor {
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
  ~ImageEditorUI() override;

 private:
  raw_ptr<Profile> profile_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace image_editor

#endif  // CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UI_H_
