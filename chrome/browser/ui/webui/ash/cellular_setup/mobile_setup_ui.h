// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_UI_H_

#include <memory>

#include "ash/constants/webui_url_constants.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class ApplicationLocaleStorage;

namespace ash::cellular_setup {

class MobileSetupUI;

class MobileSetupUIConfig : public content::WebUIConfig {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  explicit MobileSetupUIConfig(
      const ApplicationLocaleStorage* application_locale_storage);
  MobileSetupUIConfig(const MobileSetupUIConfig&) = delete;
  MobileSetupUIConfig& operator=(const MobileSetupUIConfig&) = delete;
  ~MobileSetupUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

// DEPRECATED: Being replaced by new UI; see https://crbug.com/778021.
class MobileSetupUI : public ui::WebDialogUI {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  MobileSetupUI(content::WebUI* web_ui,
                const ApplicationLocaleStorage* application_locale_storage);

  MobileSetupUI(const MobileSetupUI&) = delete;
  MobileSetupUI& operator=(const MobileSetupUI&) = delete;

  ~MobileSetupUI() override;
};

}  // namespace ash::cellular_setup

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_UI_H_
