// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/components/components_handler.h"

#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestComponentsHandler : public ComponentsHandler {
 public:
  TestComponentsHandler(
      component_updater::ComponentUpdateService* component_update_service)
      : ComponentsHandler(component_update_service) {
    set_web_ui(&test_web_ui_);
  }

 private:
  content::TestWebUI test_web_ui_;
};

TEST(ComponentsHandlerTest, RemovesObserver) {
  testing::NiceMock<component_updater::MockComponentUpdateService> mock_service;
  EXPECT_CALL(mock_service, AddObserver(testing::_)).Times(1);
  EXPECT_CALL(mock_service, RemoveObserver(testing::_)).Times(1);

  {
    TestComponentsHandler handler(&mock_service);
    base::Value::List args;
    args.Append("unused");
    handler.HandleRequestComponentsData(args);
  }
}
