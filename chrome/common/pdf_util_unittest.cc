// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_restriction.h"
#include "pdf/content_restriction.h"

static_assert(static_cast<int>(CONTENT_RESTRICTION_COPY) ==
              static_cast<int>(chrome_pdf::kContentRestrictionCopy));
static_assert(static_cast<int>(CONTENT_RESTRICTION_CUT) ==
              static_cast<int>(chrome_pdf::kContentRestrictionCut));
static_assert(static_cast<int>(CONTENT_RESTRICTION_PASTE) ==
              static_cast<int>(chrome_pdf::kContentRestrictionPaste));
static_assert(static_cast<int>(CONTENT_RESTRICTION_PRINT) ==
              static_cast<int>(chrome_pdf::kContentRestrictionPrint));
static_assert(static_cast<int>(CONTENT_RESTRICTION_SAVE) ==
              static_cast<int>(chrome_pdf::kContentRestrictionSave));
