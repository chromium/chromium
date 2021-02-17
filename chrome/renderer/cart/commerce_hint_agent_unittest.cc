// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr int kTestIterations = 1000;

// clang-format off
const char* kAddToCart[] = {
  "submit.add-to-cart: Submit",
  "gp/add-to-cart/json/",
  "-addtocart&",
  "?add-to-cart=1",
  "action: peake_add_to_basket",
  "action: woosea_addtocart_details",
  "ajax=add_to_cart",
  "action: woocommerce_add_to_cart",
  "add-to-cart: 123",
  "queryString=submit.addToCart",
  "queryString=submit.AddToCart",
  "&link=Add%20to%20Cart",
  "api_call:shopping_bag_add_to_bag", // nordstrom.com
  "\"cart_type\":\"REGULAR\"", // target.com
  "cnc/checkout/cartItems/addItemToCart", // kohls.com
  "\"event\":\"product_added_to_cart\"", // staples.com
  "checkout/basket/add_and_show", // wayfair.com
};

const char* kNotAddToCart[] = {
  "misadd-to-cart",
  "add_to_cartoon",
  "_add_to_basketball",
  "cart/address",
  "golfcart/add",
};
// clang-format on

}  // namespace

using cart::CommerceHintAgent;

TEST(CommerceHintAgentTest, IsAddToCart) {
  for (auto* str : kAddToCart) {
    EXPECT_TRUE(CommerceHintAgent::IsAddToCart(str)) << str;
  }
  for (auto* str : kNotAddToCart) {
    EXPECT_FALSE(CommerceHintAgent::IsAddToCart(str)) << str;
  }
}

float BenchmarkIsAddToCart(base::StringPiece str) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsAddToCart(str);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsAddToCart(" << str.size() << " chars) took: " << elapsed_us
            << " µs";
  return elapsed_us;
}

// TSAN builds are 20~50X slower than Release build.
#if defined(THREAD_SANITIZER) || defined(ADDRESS_SANITIZER)
#define MAYBE_RegexBenchmark DISABLED_RegexBenchmark
#else
#define MAYBE_RegexBenchmark RegexBenchmark
#endif

TEST(CommerceHintAgentTest, MAYBE_RegexBenchmark) {
  std::string str = "abcdefghijklmnop";
  for (int length = 16; length <= (1L << 20); length *= 4) {
    const GURL url("http://example.com/" + str);

    // With string copy, it would take at least 400us, assuming string length is
    // 1M chars, and running x86-64 Release build on Linux workstation. x86-64
    // Debug build is ~4X slower, Debug build on Pixel 2 is 3~10X slower, and
    // Release build on Nexus 5 is ~10X slower.
    int slow_factor = 1;
#if !defined(NDEBUG)
    slow_factor *= 4;
#endif
#if defined(OS_ANDROID)
    slow_factor *= 10;
#endif

    float elapsed_us = BenchmarkIsAddToCart(str);
    // Typical value is ~10us on x86-64 Release build.
    // Without capping the length, it would take at least 2000us.
    EXPECT_LT(elapsed_us, 50.0 * slow_factor);

    str += str;
    str += str;
  }
}
