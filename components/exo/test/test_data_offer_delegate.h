// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_TEST_DATA_OFFER_DELEGATE_H_
#define COMPONENTS_EXO_TEST_TEST_DATA_OFFER_DELEGATE_H_

#include <memory>

#include "components/exo/data_device.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/test/test_security_delegate.h"

namespace exo::test {

// A test implementation of DataOfferDelegate which persists state from
// invocations and allows inspection.
class TestDataOfferDelegate : public DataOfferDelegate {
 public:
  TestDataOfferDelegate();
  ~TestDataOfferDelegate() override;

  TestDataOfferDelegate(const TestDataOfferDelegate&) = delete;
  TestDataOfferDelegate& operator=(const TestDataOfferDelegate&) = delete;

  // DataOfferDelegate:
  void OnDataOfferDestroying(DataOffer* offer) override {}
  void OnOffer(const std::string& mime_type) override;
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override;
  void OnAction(DndAction dnd_action) override;

  const base::flat_set<std::string>& mime_types() const { return mime_types_; }
  const base::flat_set<DndAction>& source_actions() const {
    return source_actions_;
  }
  DndAction dnd_action() const { return dnd_action_; }
  TestSecurityDelegate* GetSecurityDelegate() const override;

 private:
  base::flat_set<std::string> mime_types_;
  base::flat_set<DndAction> source_actions_;
  DndAction dnd_action_ = DndAction::kNone;
  std::unique_ptr<TestSecurityDelegate> security_delegate_ =
      std::make_unique<TestSecurityDelegate>();
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_TEST_DATA_OFFER_DELEGATE_H_
