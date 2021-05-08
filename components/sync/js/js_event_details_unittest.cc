// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/js/js_event_details.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class JsEventDetailsTest : public testing::Test {};

TEST_F(JsEventDetailsTest, EmptyList) {
  JsEventDetails details;
  EXPECT_TRUE(details.Get().DictEmpty());
  EXPECT_EQ("{}", details.ToString());
}

TEST_F(JsEventDetailsTest, FromDictionary) {
  base::DictionaryValue dict;
  dict.SetString("foo", "bar");
  dict.Set("baz", std::make_unique<base::ListValue>());

  auto dict_copy = dict.Clone();

  JsEventDetails details(&dict);

  // |details| should take over |dict|'s data.
  EXPECT_TRUE(dict.DictEmpty());
  EXPECT_TRUE(details.Get().Equals(&dict_copy));
}

}  // namespace
}  // namespace syncer
