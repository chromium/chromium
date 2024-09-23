// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver_factory_test_api.h"

namespace password_manager {

// static
std::unique_ptr<ContentPasswordManagerDriverFactory>
ContentPasswordManagerDriverFactoryTestApi::Create(
    content::WebContents* web_contents,
    PasswordManagerClient* password_manager_client) {
  return base::WrapUnique(new ContentPasswordManagerDriverFactory(
      web_contents, password_manager_client));
}

// static
ContentPasswordManagerDriver*
ContentPasswordManagerDriverFactoryTestApi::GetDriverForFrame(
    ContentPasswordManagerDriverFactory* factory,
    content::RenderFrameHost* render_frame_host) {
  return factory->GetDriverForFrame(render_frame_host);
}

}  // namespace password_manager
