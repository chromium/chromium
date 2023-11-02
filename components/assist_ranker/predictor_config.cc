// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/predictor_config.h"

namespace assist_ranker {

const base::flat_set<std::string>* GetEmptyAllowlist() {
  static auto* allowlist = new base::flat_set<std::string>();
  return allowlist;
}

}  // namespace assist_ranker
