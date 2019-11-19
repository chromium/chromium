// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/common/engine_digest_verifier.h"

#include "chrome/chrome_cleaner/engines/common/engine_resources.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"

namespace chrome_cleaner {

scoped_refptr<DigestVerifier> GetDigestVerifier() {
  int resource_id = GetProtectedFilesDigestResourceId();
  return resource_id ? DigestVerifier::CreateFromResource(resource_id)
                     : nullptr;
}

}  // namespace chrome_cleaner
