// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_data_offer_delegate.h"

namespace exo::test {

TestDataOfferDelegate::TestDataOfferDelegate() = default;

TestDataOfferDelegate::~TestDataOfferDelegate() = default;

void TestDataOfferDelegate::OnOffer(const std::string& mime_type) {
  mime_types_.insert(mime_type);
}

void TestDataOfferDelegate::OnSourceActions(
    const base::flat_set<DndAction>& source_actions) {
  source_actions_ = source_actions;
}

void TestDataOfferDelegate::OnAction(DndAction dnd_action) {
  dnd_action_ = dnd_action;
}

TestSecurityDelegate* TestDataOfferDelegate::GetSecurityDelegate() const {
  return security_delegate_.get();
}

}  // namespace exo::test
