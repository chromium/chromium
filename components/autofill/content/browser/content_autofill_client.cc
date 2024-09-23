// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_client.h"

namespace autofill {

ContentAutofillClient::ContentAutofillClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ContentAutofillClient>(*web_contents),
      autofill_driver_factory_(web_contents, this) {}

ContentAutofillClient::~ContentAutofillClient() = default;

ContentAutofillDriverFactory&
ContentAutofillClient::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentAutofillClient);

}  // namespace autofill
