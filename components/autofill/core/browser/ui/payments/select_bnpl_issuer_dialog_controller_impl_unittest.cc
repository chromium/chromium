// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class SelectBnplIssuerDialogControllerImplTest : public testing::Test {
 public:
  SelectBnplIssuerDialogControllerImplTest() = default;
  ~SelectBnplIssuerDialogControllerImplTest() override = default;

  void InitController() {
    controller_ = std::make_unique<SelectBnplIssuerDialogControllerImpl>(
        issuers_, selected_issuer_callback_.Get(), cancel_callback_.Get());
  }

  void SetIssuers(std::vector<BnplIssuer> issuers) {
    issuers_ = std::move(issuers);
  }

 protected:
  std::unique_ptr<SelectBnplIssuerDialogControllerImpl> controller_;
  std::vector<BnplIssuer> issuers_;
  base::MockOnceCallback<void(const std::string&)> selected_issuer_callback_;
  base::MockOnceClosure cancel_callback_;
};

TEST_F(SelectBnplIssuerDialogControllerImplTest, Getters) {
  SetIssuers({test::GetTestLinkedBnplIssuer()});
  InitController();
  EXPECT_EQ(controller_->GetIssuers(), issuers_);
  EXPECT_CALL(selected_issuer_callback_, Run(issuers_[0].issuer_id()));
  controller_->OnAccepted(issuers_[0].issuer_id());
  EXPECT_CALL(cancel_callback_, Run());
  controller_->OnCancel();
}

}  // namespace autofill::payments
