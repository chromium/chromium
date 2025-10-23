// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/autofill_annotations_provider.h"

#include "content/public/browser/web_contents.h"

namespace optimization_guide {

namespace {

const void* const kAutofillAnnotationsProviderKey =
    &kAutofillAnnotationsProviderKey;

}  // namespace

// static
AutofillAnnotationsProvider* AutofillAnnotationsProvider::GetFor(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  return static_cast<AutofillAnnotationsProvider*>(
      web_contents->GetUserData(UserDataKey()));
}

// static
void AutofillAnnotationsProvider::SetFor(
    content::WebContents* web_contents,
    std::unique_ptr<AutofillAnnotationsProvider> provider) {
  CHECK(web_contents);
  web_contents->SetUserData(UserDataKey(), std::move(provider));
}

// static
const void* AutofillAnnotationsProvider::UserDataKey() {
  return &kAutofillAnnotationsProviderKey;
}

}  // namespace optimization_guide
