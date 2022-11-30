// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/autofill_assistant.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill_assistant {
namespace {

constexpr uint32_t kPrefixSize = 15u;
constexpr char kUrl1[] = "https://www.example-page1.co.uk/further_path/";
constexpr char kUrl2[] = "https://www.example-page1.co.uk/";
constexpr char kUrl3[] = "https://www.example-page1.co.uk";
constexpr uint64_t kHash = 30578;

TEST(AutofillAssistantTest, GetHashPrefix) {
  EXPECT_EQ(AutofillAssistant::GetHashPrefix(kPrefixSize,
                                             url::Origin::Create(GURL(kUrl1))),
            kHash);
  EXPECT_EQ(AutofillAssistant::GetHashPrefix(kPrefixSize,
                                             url::Origin::Create(GURL(kUrl2))),
            kHash);
  EXPECT_EQ(AutofillAssistant::GetHashPrefix(kPrefixSize,
                                             url::Origin::Create(GURL(kUrl3))),
            kHash);
}

}  // namespace

}  // namespace autofill_assistant
