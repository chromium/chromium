// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"

namespace page_actions {

MockPageActionController::MockPageActionController() {
  ON_CALL(*this, AddObserver(testing::_, testing::_))
      .WillByDefault(
          [&](actions::ActionId id,
              base::ScopedObservation<PageActionModelInterface,
                                      PageActionModelObserver>& observation) {
            observation.Observe(&model_);
          });
}

MockPageActionController::~MockPageActionController() = default;

}  // namespace page_actions
