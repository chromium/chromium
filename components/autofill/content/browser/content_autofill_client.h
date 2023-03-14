// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_CLIENT_H_

#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Common base class for those AutofillClients that have the //content layer.
class ContentAutofillClient
    : public AutofillClient,
      public content::WebContentsUserData<ContentAutofillClient> {
 public:
  ContentAutofillClient(
      content::WebContents* web_contents,
      ContentAutofillDriverFactory::DriverInitCallback driver_init_hook);
  ContentAutofillClient(const ContentAutofillClient&) = delete;
  ContentAutofillClient& operator=(const ContentAutofillClient&) = delete;
  ~ContentAutofillClient() override;

  // Intentionally non-virtual to allow it to be called during construction (in
  // particular, transitively by members of subclasses).
  ContentAutofillDriverFactory* GetAutofillDriverFactory();

 private:
  friend class content::WebContentsUserData<ContentAutofillClient>;

  ContentAutofillDriverFactory autofill_driver_factory_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_CLIENT_H_
