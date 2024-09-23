// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/privileged/mojom/bucket_client_info_mojom_traits.h"

#include "base/unguessable_token.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/bucket_client_info.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace storage {
namespace {

TEST(BucketClientInfoMojomTraitsTest, SerializeAndDeserializeExpectedToken) {
  const int process_id = 1;
  BucketClientInfo test_objects[] = {
      BucketClientInfo{process_id,
                       blink::LocalFrameToken(base::UnguessableToken::Create()),
                       blink::DocumentToken(base::UnguessableToken::Create())},
      BucketClientInfo{
          process_id,
          blink::DedicatedWorkerToken(base::UnguessableToken::Create()),
          blink::DocumentToken(base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::DedicatedWorkerToken(
                                       base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::SharedWorkerToken(
                                       base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::ServiceWorkerToken(
                                       base::UnguessableToken::Create())},
  };

  for (auto& original : test_objects) {
    BucketClientInfo copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BucketClientInfo>(
        original, copied));
    EXPECT_EQ(original.process_id, copied.process_id);
    EXPECT_EQ(original.context_token, copied.context_token);
    EXPECT_EQ(original.document_token, copied.document_token);
  }
}

TEST(BucketClientInfoMojomTraitsTest, FailUnexpectedContextTokenType) {
  const int process_id = 1;
  BucketClientInfo test_objects[] = {
      BucketClientInfo{process_id, blink::AnimationWorkletToken(
                                       base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::AudioWorkletToken(
                                       base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::LayoutWorkletToken(
                                       base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::PaintWorkletToken(
                                       base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::SharedStorageWorkletToken(
                                       base::UnguessableToken::Create())},
      BucketClientInfo{process_id, blink::ShadowRealmToken(
                                       base::UnguessableToken::Create())},
  };

  for (auto& original : test_objects) {
    BucketClientInfo copied;
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BucketClientInfo>(
        original, copied));
  }
}

TEST(BucketClientInfoMojomTraitsTest, FailUnexpectedDocumentToken) {
  const int process_id = 1;
  BucketClientInfo test_objects[] = {
      BucketClientInfo{
          process_id,
          blink::ServiceWorkerToken(base::UnguessableToken::Create()),
          blink::DocumentToken(base::UnguessableToken::Create())},
      BucketClientInfo{
          process_id,
          blink::SharedWorkerToken(base::UnguessableToken::Create()),
          blink::DocumentToken(base::UnguessableToken::Create())},
  };

  for (auto& original : test_objects) {
    BucketClientInfo copied;
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BucketClientInfo>(
        original, copied));
  }
}

}  // namespace
}  // namespace storage
