// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/content_capture/common/traits_test_service.test-mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_capture {
namespace {

class ContentCaptureStructTraitsTest : public testing::Test,
                                       public mojom::TraitsTestService {
 public:
  ContentCaptureStructTraitsTest() = default;

 protected:
  mojo::Remote<mojom::TraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::TraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  // TraitsTestService:
  void EchoContentCaptureData(
      const ContentCaptureData& i,
      EchoContentCaptureDataCallback callback) override {
    std::move(callback).Run(i);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;

  DISALLOW_COPY_AND_ASSIGN(ContentCaptureStructTraitsTest);
};

TEST_F(ContentCaptureStructTraitsTest, ContentCaptureData) {
  ContentCaptureData child;
  child.id = 2;
  child.value = base::ASCIIToUTF16("Hello");
  child.bounds = gfx::Rect(5, 5, 5, 5);
  ContentCaptureData input;
  input.id = 1;
  input.value = base::ASCIIToUTF16("http://foo.com/bar");
  input.bounds = gfx::Rect(10, 10);
  input.children.push_back(child);

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  ContentCaptureData output;
  remote->EchoContentCaptureData(input, &output);
  EXPECT_EQ(input, output);
}

}  // namespace
}  // namespace content_capture
