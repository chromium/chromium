// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/fake_mahi_manager.h"

#include <algorithm>

#include "base/functional/callback.h"

namespace chromeos {

void FakeMahiManager::GetSummary(MahiSummaryCallback callback) {
  std::move(callback).Run(summary_text_);
}

}  // namespace chromeos
