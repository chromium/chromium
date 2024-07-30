// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_MOCK_CHOOSER_CONTROLLER_VIEW_H_
#define COMPONENTS_PERMISSIONS_MOCK_CHOOSER_CONTROLLER_VIEW_H_

#include "components/permissions/chooser_controller.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace permissions {

class MockChooserControllerView : public ChooserController::View {
 public:
  MockChooserControllerView();
  MockChooserControllerView(MockChooserControllerView&) = delete;
  MockChooserControllerView& operator=(MockChooserControllerView&) = delete;
  ~MockChooserControllerView() override;

  // ChooserController::View
  MOCK_METHOD0(OnOptionsInitialized, void());
  MOCK_METHOD1(OnOptionAdded, void(size_t index));
  MOCK_METHOD1(OnOptionRemoved, void(size_t index));
  MOCK_METHOD1(OnOptionUpdated, void(size_t index));
  MOCK_METHOD1(OnAdapterEnabledChanged, void(bool enabled));
  MOCK_METHOD1(OnAdapterAuthorizationChanged, void(bool enabled));
  MOCK_METHOD1(OnRefreshStateChanged, void(bool enabled));
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_MOCK_CHOOSER_CONTROLLER_VIEW_H_
