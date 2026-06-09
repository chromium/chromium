// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_PRODUCT_MESSAGING_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_PRODUCT_MESSAGING_CONTROLLER_H_

#include <initializer_list>

#include "components/user_education/common/product_messaging_controller.h"

namespace user_education::test {

// Simulates a notice that requests to show in the `ProductMessagingController`.
// Will hold the handle until `Release()` is called.
class TestProductMessage {
 public:
  explicit TestProductMessage(
      ProductMessagingController& controller,
      ProductMessageKey key,
      std::initializer_list<ProductMessageKey> show_after = {},
      std::initializer_list<ProductMessageKey> blocked_by = {});
  TestProductMessage(const TestProductMessage&) = delete;
  void operator=(const TestProductMessage&) = delete;
  ~TestProductMessage();

  // Mark that the notice was shown.
  void SetShown();

  // Release the handle (which must be held).
  void Release();

  ProductMessageKey key() const { return key_; }
  bool received_priority() const { return shown_; }
  bool has_priority() const { return static_cast<bool>(handle_); }

 private:
  void OnReadyToShow(ProductMessagingHandle handle);

  const ProductMessageKey key_;
  bool shown_ = false;
  ProductMessagingHandle handle_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_PRODUCT_MESSAGING_CONTROLLER_H_
