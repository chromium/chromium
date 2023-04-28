// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_HOME_MOCK_APP_HOME_PAGE_H_
#define CHROME_BROWSER_UI_WEBUI_APP_HOME_MOCK_APP_HOME_PAGE_H_

#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webapps {

class MockAppHomePage : public app_home::mojom::Page {
 public:
  MockAppHomePage();
  ~MockAppHomePage() override;

  mojo::PendingRemote<app_home::mojom::Page> BindAndGetRemote();

  MOCK_METHOD(void, AddApp, (app_home::mojom::AppInfoPtr));
  MOCK_METHOD(void, RemoveApp, (app_home::mojom::AppInfoPtr));
  MOCK_METHOD(void, EnableExtensionApp, (const std::string&));
  mojo::Receiver<app_home::mojom::Page> receiver_{this};
};

}  // namespace webapps

#endif  // CHROME_BROWSER_UI_WEBUI_APP_HOME_MOCK_APP_HOME_PAGE_H_
