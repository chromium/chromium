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

const char* kVisitCart[] = {
  "https://www.amazon.com/gp/aw/c?ref_=navm_hdr_cart",
  "https://smile.amazon.com/gp/aw/c?ref_=navm_hdr_cart",
  "https://www.amazon.com/gp/aws/cart/add.html",
  "https://smile.amazon.com/gp/cart/view.html",
  "https://www.amazon.com/gp/cart/view.html/ref=lh_cart",
  "https://www.amazon.com/-/es/gp/cart/view.html",
  "https://cart.ebay.com/",
  "https://cart.payments.ebay.com/sc/add",
  "https://www.etsy.com/cart/listing.php",
  "https://www.target.com/co-cart",
  "https://secure2.homedepot.com/mycart/home",
  "http://example.com/us/cart/",
  "http://example.com/cart/",
  "https://example.com/cart",
  "http://example.com/cart",
};

const char* kNotVisitCart[] = {
  "https://www.amazon.com/gp/aw/changed?ref_=navm_hdr_cart",
  "http://example.com/gp/aw/c?ref_=navm_hdr_cart",
  "http://example.com/cartoon",
};

const char* kVisitCheckout[] = {
  "https://www.amazon.com/gp/cart/mobile/go-to-checkout.html/ref=ox_sc_proceed?proceedToCheckout=1",
  "https://smile.amazon.com/gp/cart/mobile/go-to-checkout.html/ref=ox_sc_proceed?proceedToCheckout=1",
  "http://example.com/us/checkout/",
  "http://example.com/checkout/",
  "https://example.com/123/checkouts/456",
  "http://example.com/123/checkouts/456",
};

const char* kNotVisitCheckout[] = {
  "https://example.com/gp/cart/mobile/go-to-checkout.html/ref=ox_sc_proceed?proceedToCheckout=1",
  "http://example.com/checkoutput",
  "http://example.com/us/checkoutside/",
};

const char* kPurchaseURL[] = {
  "https://www.amazon.com/gp/buy/spc/handlers/static-submit-decoupled.html/ref=ox_spc_place_order?ie=UTF8",
};

const char* kNotPurchaseURL[] = {
  "https://example.com/gp/buy/spc/handlers/static-submit-decoupled.html/ref=ox_spc_place_order?ie=UTF8",
};

const char* kPurchaseText[] = {
  "Place order",
  "PAY NOW",
  "PLACE ORDER",
  "Pay now",
};

const char* kNotPurchaseText[] = {
  "I'd like to pay now",
  "replace order",
  "Pay nowadays",
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

TEST(CommerceHintAgentTest, IsVisitCart) {
  for (auto* str : kVisitCart) {
    EXPECT_TRUE(CommerceHintAgent::IsVisitCart(GURL(str))) << str;
  }
  for (auto* str : kNotVisitCart) {
    EXPECT_FALSE(CommerceHintAgent::IsVisitCart(GURL(str))) << str;
  }
}

TEST(CommerceHintAgentTest, IsVisitCheckout) {
  for (auto* str : kVisitCheckout) {
    EXPECT_TRUE(CommerceHintAgent::IsVisitCheckout(GURL(str))) << str;
  }
  for (auto* str : kNotVisitCheckout) {
    EXPECT_FALSE(CommerceHintAgent::IsVisitCheckout(GURL(str))) << str;
  }
}

TEST(CommerceHintAgentTest, IsPurchaseByURL) {
  for (auto* str : kPurchaseURL) {
    EXPECT_TRUE(CommerceHintAgent::IsPurchase(GURL(str))) << str;
  }
  for (auto* str : kNotPurchaseURL) {
    EXPECT_FALSE(CommerceHintAgent::IsPurchase(GURL(str))) << str;
  }
}

TEST(CommerceHintAgentTest, IsPurchaseByForm) {
  for (auto* str : kPurchaseText) {
    EXPECT_TRUE(CommerceHintAgent::IsPurchase(str)) << str;
  }
  for (auto* str : kNotPurchaseText) {
    EXPECT_FALSE(CommerceHintAgent::IsPurchase(str)) << str;
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

float BenchmarkIsVisitCart(const GURL& url) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsVisitCart(url);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsVisitCart(" << url.spec().size()
            << " chars) took: " << elapsed_us << " µs";
  return elapsed_us;
}

float BenchmarkIsVisitCheckout(const GURL& url) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsVisitCheckout(url);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsVisitCheckout(" << url.spec().size()
            << " chars) took: " << elapsed_us << " µs";
  return elapsed_us;
}

float BenchmarkIsPurchase(base::StringPiece str) {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    CommerceHintAgent::IsPurchase(str);
  }
  const base::TimeTicks end = base::TimeTicks::Now();
  float elapsed_us =
      static_cast<float>((end - now).InMicroseconds()) / kTestIterations;
  LOG(INFO) << "IsPurchase(" << str.size() << " chars) took: " << elapsed_us
            << " µs";
  return elapsed_us;
}

// TSAN builds are 20~50X slower than Release build.
#if defined(THREAD_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
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

    elapsed_us = BenchmarkIsVisitCart(url);
    // Typical value is ~10us.
    EXPECT_LT(elapsed_us, 50.0 * slow_factor);

    elapsed_us = BenchmarkIsVisitCheckout(url);
    // Typical value is ~0.2us.
    // Without capping the length, it would take at least 30us.
    EXPECT_LT(elapsed_us, 2.0 * slow_factor);

    elapsed_us = BenchmarkIsPurchase(str);
    // Typical value is ~0.1us.
    EXPECT_LT(elapsed_us, 1.0 * slow_factor);

    str += str;
    str += str;
  }
}
