// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_product_messaging_controller.h"

#include "components/user_education/common/product_messaging_controller.h"

namespace user_education::test {

TestProductMessage::TestProductMessage(
    ProductMessagingController& controller,
    ProductMessageKey key,
    std::initializer_list<ProductMessageKey> show_after,
    std::initializer_list<ProductMessageKey> blocked_by)
    : key_(key) {
  controller.QueueMessage(key_,
                          base::BindOnce(&TestProductMessage::OnReadyToShow,
                                         base::Unretained(this)),
                          show_after, blocked_by);
}

TestProductMessage::~TestProductMessage() = default;

void TestProductMessage::SetShown() {
  CHECK(handle_);
  handle_->SetShown();
}

void TestProductMessage::Release() {
  CHECK(handle_);
  handle_.reset();
}

void TestProductMessage::OnReadyToShow(ProductMessagingHandle handle) {
  CHECK(handle);
  shown_ = true;
  handle_ = std::move(handle);
}

}  // namespace user_education::test
