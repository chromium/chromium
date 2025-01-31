// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using base::MockCallback;
using base::OnceCallback;
using testing::ByMove;
using testing::Return;
using testing::Test;

class BnplTosControllerImplTest : public Test {
 public:
  void SetUp() override {
    controller_ = std::make_unique<BnplTosControllerImpl>();
    std::unique_ptr<BnplTosView> view = std::make_unique<BnplTosView>();
    view_ = view.get();
    ON_CALL(mock_callback_, Run).WillByDefault(Return(ByMove(std::move(view))));
  }

  void TearDown() override {
    // Avoid dangling pointer.
    view_ = nullptr;
  }

  void ShowView() { controller_->Show(mock_callback_.Get()); }

  BnplTosView* View() { return controller_->view_.get(); }

  std::unique_ptr<BnplTosControllerImpl> controller_;
  MockCallback<OnceCallback<std::unique_ptr<BnplTosView>()>> mock_callback_;
  raw_ptr<BnplTosView> view_;
};

TEST_F(BnplTosControllerImplTest, ShowView) {
  EXPECT_CALL(mock_callback_, Run());
  ShowView();
  EXPECT_EQ(View(), view_);
}

TEST_F(BnplTosControllerImplTest, ShowView_MultipleTimes) {
  EXPECT_CALL(mock_callback_, Run()).Times(1);
  ShowView();
  ShowView();
}

TEST_F(BnplTosControllerImplTest, OnClosed) {
  EXPECT_CALL(mock_callback_, Run());
  ShowView();

  // Avoid dangling pointer.
  view_ = nullptr;

  controller_->OnViewClosing();
  EXPECT_EQ(View(), nullptr);
}

}  // namespace autofill
