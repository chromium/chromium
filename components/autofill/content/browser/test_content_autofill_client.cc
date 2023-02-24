// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/test_content_autofill_client.h"

#include "components/autofill/core/browser/browser_autofill_manager.h"

namespace autofill {

TestContentAutofillClient::TestContentAutofillClient(
    content::WebContents* web_contents)
    : TestContentAutofillClient(
          web_contents,
          base::BindRepeating(&BrowserDriverInitHook, this, "en-US")) {}

}  // namespace autofill
