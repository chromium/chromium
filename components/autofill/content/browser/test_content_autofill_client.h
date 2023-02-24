// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_CLIENT_H_

#include "base/functional/bind.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// A variant of TestAutofillClient that can be associated with a
// content::WebContents.
//
// The typical pattern to associate it with a content::WebContents:
//   auto client = std::make_unique<TestContentAutofillClient>(web_contents);
//   web_contents->SetUserData(client->UserData(), std::move(client));
//   ...
//   web_contents->RemoveUserData(client->UserData());
class TestContentAutofillClient
    : public TestAutofillClientTemplate<ContentAutofillClient> {
 public:
  explicit TestContentAutofillClient(content::WebContents* web_contents);

  using TestAutofillClientTemplate<
      ContentAutofillClient>::TestAutofillClientTemplate;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_CONTENT_AUTOFILL_CLIENT_H_
