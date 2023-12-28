// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_util.h"

#include "base/check_op.h"
#include "base/unguessable_token.h"

namespace metrics::structured::util {

std::string GenerateNewKey() {
  const std::string key = base::UnguessableToken::Create().ToString();
  CHECK_EQ(key.size(), kKeySize);
  return key;
}

}  // namespace metrics::structured::util
