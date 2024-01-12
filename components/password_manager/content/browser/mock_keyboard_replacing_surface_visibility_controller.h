// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_MOCK_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_MOCK_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockKeyboardReplacingSurfaceVisibilityController final
    : public KeyboardReplacingSurfaceVisibilityController {
 public:
  MockKeyboardReplacingSurfaceVisibilityController();
  ~MockKeyboardReplacingSurfaceVisibilityController() override;

  MOCK_METHOD(bool, CanBeShown, (), (const override));
  MOCK_METHOD(bool, IsVisible, (), (const override));
  MOCK_METHOD(void,
              SetVisible,
              (base::WeakPtr<password_manager::ContentPasswordManagerDriver>
                   frame_driver),
              (override));
  MOCK_METHOD(void, SetShown, (), (override));
  MOCK_METHOD(void, Reset, (), (override));

  base::WeakPtr<KeyboardReplacingSurfaceVisibilityController> AsWeakPtr()
      override;

 private:
  base::WeakPtrFactory<MockKeyboardReplacingSurfaceVisibilityController>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_MOCK_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_
