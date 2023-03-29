// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "components/chrome_cleaner/public/constants/result_codes.h"

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
  return chrome_cleaner::RESULT_CODE_NO_PUPS_FOUND;
}
