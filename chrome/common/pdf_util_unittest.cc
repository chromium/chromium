// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pdf_util.h"

#include "chrome/common/content_restriction.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/content_restriction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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

TEST(PdfUtil, IsPdfExtensionOrigin) {
  EXPECT_FALSE(
      IsPdfExtensionOrigin(url::Origin::Create(GURL("https://example.com/"))));
  EXPECT_FALSE(IsPdfExtensionOrigin(
      url::Origin::Create(GURL("https://mhjfbmdgcfjbbpaeojofohoefgiehjai/"))));
  EXPECT_FALSE(IsPdfExtensionOrigin(
      url::Origin::Create(GURL("chrome-extension://foo/"))));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(IsPdfExtensionOrigin(url::Origin::Create(
      GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/"))));
#else
  EXPECT_FALSE(IsPdfExtensionOrigin(url::Origin::Create(
      GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/"))));
#endif
}
