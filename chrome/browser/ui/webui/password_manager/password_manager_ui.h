// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

namespace extensions {
class PasswordsPrivateDelegate;
}

class PasswordManagerUI : public content::WebUIController {
 public:
  explicit PasswordManagerUI(content::WebUI* web_ui);
  ~PasswordManagerUI() override;

  PasswordManagerUI(const PasswordManagerUI&) = delete;
  PasswordManagerUI& operator=(const PasswordManagerUI&) = delete;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

 private:
  scoped_refptr<extensions::PasswordsPrivateDelegate>
      passwords_private_delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_H_
