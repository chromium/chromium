// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_DIGEST_VERIFIER_H_
#define CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_DIGEST_VERIFIER_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/chrome_cleaner/engines/common/engine_digest_verifier.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"

namespace chrome_cleaner {

// Creates a DigestVerifier from the protected files resource id. Returns
// nullptr if the resource id is not set.
scoped_refptr<DigestVerifier> GetDigestVerifier();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_DIGEST_VERIFIER_H_
