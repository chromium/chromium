// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"

#include "base/strings/string_piece.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(BorrowedTransliterator, RemoveDiacriticsAndConvertToLowerCase) {
  EXPECT_EQ(RemoveDiacriticsAndConvertToLowerCase(
                u"āēaa11.īūčģķļņšžKāäǟḑēīļņōȯȱõȭŗšțūž"),
            u"aeaa11.iucgklnszkaaadeilnooooorstuz");
}

}  // namespace autofill
