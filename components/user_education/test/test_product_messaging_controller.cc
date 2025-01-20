// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_product_messaging_controller.h"

#include "components/user_education/common/product_messaging_controller.h"

namespace user_education::test {

TestNotice::TestNotice(ProductMessagingController& controller,
                       RequiredNoticeId id,
                       std::initializer_list<RequiredNoticeId> show_after,
                       std::initializer_list<RequiredNoticeId> blocked_by)
    : id_(id) {
  controller.QueueRequiredNotice(
      id_, base::BindOnce(&TestNotice::OnReadyToShow, base::Unretained(this)),
      show_after, blocked_by);
}

TestNotice::~TestNotice() = default;

void TestNotice::SetShown() {
  CHECK(handle_);
  handle_.SetShown();
}

void TestNotice::Release() {
  CHECK(handle_);
  handle_.Release();
}

void TestNotice::OnReadyToShow(RequiredNoticePriorityHandle handle) {
  CHECK(handle);
  shown_ = true;
  handle_ = std::move(handle);
}

}  // namespace user_education::test
