// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_processing_util.h"

#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/store_update_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

TEST(HintsProcessingUtilTest, FindPageHintForSubstringPagePattern) {
  proto::Hint hint1;

  // Page hint for "/one/"
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("foo.org/*/one/");

  // Page hint for "two"
  proto::PageHint* page_hint2 = hint1.add_page_hints();
  page_hint2->set_page_pattern("two");

  // Page hint for "three.jpg"
  proto::PageHint* page_hint3 = hint1.add_page_hints();
  page_hint3->set_page_pattern("three.jpg");

  EXPECT_EQ(nullptr, FindPageHintForURL(GURL(""), &hint1));
  EXPECT_EQ(nullptr, FindPageHintForURL(GURL("https://www.foo.org/"), &hint1));
  EXPECT_EQ(nullptr,
            FindPageHintForURL(GURL("https://www.foo.org/one"), &hint1));

  EXPECT_EQ(nullptr,
            FindPageHintForURL(GURL("https://www.foo.org/one/"), &hint1));
  EXPECT_EQ(page_hint1,
            FindPageHintForURL(GURL("https://www.foo.org/pages/one/"), &hint1));
  EXPECT_EQ(page_hint1,
            FindPageHintForURL(GURL("https://www.foo.org/pages/subpages/one/"),
                               &hint1));
  EXPECT_EQ(page_hint1, FindPageHintForURL(
                            GURL("https://www.foo.org/pages/one/two"), &hint1));
  EXPECT_EQ(page_hint1,
            FindPageHintForURL(
                GURL("https://www.foo.org/pages/one/two/three.jpg"), &hint1));

  EXPECT_EQ(page_hint2,
            FindPageHintForURL(
                GURL("https://www.foo.org/pages/onetwo/three.jpg"), &hint1));
  EXPECT_EQ(page_hint2,
            FindPageHintForURL(GURL("https://www.foo.org/one/two/three.jpg"),
                               &hint1));
  EXPECT_EQ(page_hint2,
            FindPageHintForURL(GURL("https://one.two.org"), &hint1));

  EXPECT_EQ(page_hint3, FindPageHintForURL(
                            GURL("https://www.foo.org/bar/three.jpg"), &hint1));
}

TEST(HintsProcessingUtilTest, ProcessHintsNoUpdateData) {
  proto::Hint hint;
  hint.set_key("whatever.com");
  hint.set_key_representation(proto::HOST_SUFFIX);
  proto::PageHint* page_hint = hint.add_page_hints();
  page_hint->set_page_pattern("foo.org/*/one/");

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = hint;

  EXPECT_FALSE(ProcessHints(&hints, nullptr));
}

TEST(HintsProcessingUtilTest, ProcessHintsWithNoPageHintsAndUpdateData) {
  proto::Hint hint;
  hint.set_key("whatever.com");
  hint.set_key_representation(proto::HOST_SUFFIX);

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = hint;

  std::unique_ptr<StoreUpdateData> update_data =
      StoreUpdateData::CreateComponentStoreUpdateData(base::Version("1.0.0"));
  EXPECT_FALSE(ProcessHints(&hints, update_data.get()));
  // Verify there is 1 store entries: 1 for the metadata entry.
  EXPECT_EQ(1ul, update_data->TakeUpdateEntries()->size());
}

TEST(HintsProcessingUtilTest, ProcessHintsWithPageHintsAndUpdateData) {
  google::protobuf::RepeatedPtrField<proto::Hint> hints;

  proto::Hint hint;
  hint.set_key("foo.org");
  hint.set_key_representation(proto::HOST_SUFFIX);
  proto::PageHint* page_hint = hint.add_page_hints();
  page_hint->set_page_pattern("foo.org/*/one/");
  *(hints.Add()) = hint;

  proto::Hint no_host_suffix_hint;
  no_host_suffix_hint.set_key("foo2.org");
  proto::PageHint* page_hint3 = no_host_suffix_hint.add_page_hints();
  page_hint3->set_page_pattern("foo2.org/blahh");
  *(hints.Add()) = no_host_suffix_hint;

  proto::Hint no_page_hints_hint;
  no_page_hints_hint.set_key("whatever.com");
  no_page_hints_hint.set_key_representation(proto::HOST_SUFFIX);
  *(hints.Add()) = no_page_hints_hint;

  std::unique_ptr<StoreUpdateData> update_data =
      StoreUpdateData::CreateComponentStoreUpdateData(base::Version("1.0.0"));
  EXPECT_TRUE(ProcessHints(&hints, update_data.get()));
  // Verify there are 2 store entries: 1 for the metadata entry plus
  // the 1 added hint entries.
  EXPECT_EQ(2ul, update_data->TakeUpdateEntries()->size());
}

TEST(HintsProcessingUtilTest, ConvertProtoEffectiveConnectionType) {
  EXPECT_EQ(
      ConvertProtoEffectiveConnectionType(
          proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  EXPECT_EQ(
      ConvertProtoEffectiveConnectionType(
          proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_OFFLINE),
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  EXPECT_EQ(
      ConvertProtoEffectiveConnectionType(
          proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G),
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  EXPECT_EQ(ConvertProtoEffectiveConnectionType(
                proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G),
            net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(ConvertProtoEffectiveConnectionType(
                proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G),
            net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(ConvertProtoEffectiveConnectionType(
                proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G),
            net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
}

}  // namespace optimization_guide
