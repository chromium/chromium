// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/test_page_action_properties_provider.h"

#include "base/check.h"
#include "ui/actions/action_id.h"

namespace page_actions {

TestPageActionPropertiesProvider::TestPageActionPropertiesProvider(
    const PageActionPropertiesMap& properties)
    : properties_(std::move(properties)) {}

TestPageActionPropertiesProvider::~TestPageActionPropertiesProvider() = default;

const PageActionProperties& TestPageActionPropertiesProvider::GetProperties(
    actions::ActionId action_id) const {
  CHECK(properties_.contains(action_id));
  return properties_.at(action_id);
}

}  // namespace page_actions
