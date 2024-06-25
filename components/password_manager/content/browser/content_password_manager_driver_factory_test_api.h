// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_TEST_API_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_TEST_API_H_

#include <memory>

#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"

namespace password_manager {

class ContentPasswordManagerDriverFactoryTestApi {
 public:
  static std::unique_ptr<ContentPasswordManagerDriverFactory> Create(
      content::WebContents* web_contents,
      PasswordManagerClient* password_manager_client);

  static ContentPasswordManagerDriver* GetDriverForFrame(
      ContentPasswordManagerDriverFactory* factory,
      content::RenderFrameHost* render_frame_host);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_TEST_API_H_
