// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/fake_mahi_manager.h"

#include <algorithm>

#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

std::u16string FakeMahiManager::GetContentTitle() {
  return u"fake content title";
}

gfx::ImageSkia FakeMahiManager::GetContentIcon() {
  return gfx::ImageSkia();
}

void FakeMahiManager::GetSummary(MahiSummaryCallback callback) {
  std::move(callback).Run(summary_text_);
}

}  // namespace chromeos
