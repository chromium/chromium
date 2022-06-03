// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_UNTRUSTED_WEB_UI_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_UNTRUSTED_WEB_UI_CONTROLLER_FACTORY_H_

#include "ui/webui/untrusted_web_ui_controller_factory.h"

class ChromeUntrustedWebUIControllerFactory
    : public ui::UntrustedWebUIControllerFactory {
 public:
  // Register the singleton instance of this class.
  static void RegisterInstance();

  ChromeUntrustedWebUIControllerFactory();
  ChromeUntrustedWebUIControllerFactory(
      const ChromeUntrustedWebUIControllerFactory&) = delete;
  ChromeUntrustedWebUIControllerFactory& operator=(
      const ChromeUntrustedWebUIControllerFactory&) = delete;

 protected:
  const WebUIConfigMap& GetWebUIConfigMap() override;

 private:
  ~ChromeUntrustedWebUIControllerFactory() override;
  WebUIConfigMap configs_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_UNTRUSTED_WEB_UI_CONTROLLER_FACTORY_H_
