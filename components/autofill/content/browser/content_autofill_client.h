// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_CLIENT_H_

#include "base/types/pass_key.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace autofill {

// Common base class for those AutofillClients that have the //content layer.
class ContentAutofillClient
    : public AutofillClient,
      public content::WebContentsUserData<ContentAutofillClient> {
 public:
  explicit ContentAutofillClient(content::WebContents* web_contents);
  ContentAutofillClient(const ContentAutofillClient&) = delete;
  ContentAutofillClient& operator=(const ContentAutofillClient&) = delete;
  ~ContentAutofillClient() override;

  // Intentionally final to allow it to be called during construction (in
  // particular, transitively by members of subclasses).
  ContentAutofillDriverFactory& GetAutofillDriverFactory() final;

  // Called by ContentAutofillDriver's constructor to inject embedder-specific
  // behaviour. Implementations should not call into `driver`.
  virtual std::unique_ptr<AutofillManager> CreateManager(
      base::PassKey<ContentAutofillDriver> pass_key,
      ContentAutofillDriver& driver) = 0;

 private:
  friend class content::WebContentsUserData<ContentAutofillClient>;

  ContentAutofillDriverFactory autofill_driver_factory_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_CLIENT_H_
