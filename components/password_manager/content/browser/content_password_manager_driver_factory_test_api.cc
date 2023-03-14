// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver_factory_test_api.h"

#include "components/password_manager/content/browser/content_password_manager_driver.h"

namespace password_manager {

// static
std::unique_ptr<ContentPasswordManagerDriverFactory>
ContentPasswordManagerDriverFactoryTestApi::Create(
    content::WebContents* web_contents,
    PasswordManagerClient* password_manager_client,
    autofill::AutofillClient* autofill_client) {
  return base::WrapUnique(new ContentPasswordManagerDriverFactory(
      web_contents, password_manager_client, autofill_client));
}

ContentPasswordManagerDriverFactoryTestApi::
    ContentPasswordManagerDriverFactoryTestApi(
        ContentPasswordManagerDriverFactory* factory)
    : factory_(factory) {}

void ContentPasswordManagerDriverFactoryTestApi::SetAutofillClient(
    autofill::AutofillClient* autofill_client) {
  factory_->autofill_client_ = autofill_client;
  for (auto& [rfh, driver] : factory_->frame_driver_map_) {
    driver.GetPasswordAutofillManager()->set_autofill_client_for_test(
        autofill_client);
  }
}

}  // namespace password_manager
