// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/common/pdf_util.h"

#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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
