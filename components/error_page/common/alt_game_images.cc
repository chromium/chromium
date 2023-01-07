// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/alt_game_images.h"

#include "base/notreached.h"

namespace error_page {

bool EnableAltGameMode() {
  return false;
}

std::string GetAltGameImage(int image_id, int scale) {
  NOTIMPLEMENTED();
  return std::string();
}

int ChooseAltGame() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace error_page
