// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/web_message_mojom_traits.h"

#include <variant>

#include "build/build_config.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace js_injection {

TEST(WebMessageMojomTraitsTest, StringRoundTrip) {
  const std::u16string kString = u"hello";
  blink::WebMessagePayload payload = kString;

  blink::WebMessagePayload output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JsWebMessage>(payload,
                                                                       output));

  EXPECT_EQ(kString, std::get<std::u16string>(output));
}

TEST(WebMessageMojomTraitsTest, ArrayBufferRoundTrip) {
  std::vector<uint8_t> kArrayBuffer = {0x01, 0x02, 0x03, 0x04, 0x05};
  blink::WebMessagePayload payload =
      blink::WebMessageArrayBufferPayload::CreateForTesting(kArrayBuffer);

  blink::WebMessagePayload output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JsWebMessage>(payload,
                                                                       output));

  auto& array_buffer_output =
      std::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(output);
  std::vector<uint8_t> output_vector(array_buffer_output->GetLength());
  array_buffer_output->CopyInto(output_vector);
  EXPECT_EQ(kArrayBuffer, output_vector);
}

#if BUILDFLAG(IS_ANDROID)
TEST(WebMessageMojomTraitsTest, SharedArrayBufferRoundTrip) {
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(1024);
  ASSERT_TRUE(region.IsValid());
  base::WritableSharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  mapping.GetMemoryAsSpan<uint8_t>()[0] = 42;

  blink::WebMessagePayload payload = std::move(region);

  blink::WebMessagePayload output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::JsWebMessage>(payload,
                                                                       output));

  ASSERT_TRUE(std::holds_alternative<base::UnsafeSharedMemoryRegion>(output));
  base::UnsafeSharedMemoryRegion output_region =
      std::get<base::UnsafeSharedMemoryRegion>(std::move(output));
  ASSERT_TRUE(output_region.IsValid());
  base::WritableSharedMemoryMapping output_mapping = output_region.Map();
  ASSERT_TRUE(output_mapping.IsValid());
  EXPECT_EQ(42, output_mapping.GetMemoryAsSpan<uint8_t>()[0]);
}
#endif

}  // namespace js_injection
