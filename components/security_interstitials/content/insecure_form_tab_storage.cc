// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/insecure_form_tab_storage.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"

namespace security_interstitials {

// Arbitrary but unique key required for SupportsUserData.
const void* const kInsecureFormTabStorageKey = &kInsecureFormTabStorageKey;

InsecureFormTabStorage::InsecureFormTabStorage(content::WebContents* contents)
    : content::WebContentsUserData<InsecureFormTabStorage>(*contents) {}

InsecureFormTabStorage::~InsecureFormTabStorage() = default;

// static
InsecureFormTabStorage* InsecureFormTabStorage::GetOrCreate(
    content::WebContents* web_contents) {
  InsecureFormTabStorage* storage = FromWebContents(web_contents);
  if (!storage) {
    CreateForWebContents(web_contents);
    storage = FromWebContents(web_contents);
  }
  return storage;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(InsecureFormTabStorage);

}  // namespace security_interstitials
