// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_service.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Error base class that keeps track of the number of errors that exist.
class BaseError : public GlobalError {
 public:
  BaseError() { ++count_; }

  BaseError(const BaseError&) = delete;
  BaseError& operator=(const BaseError&) = delete;

  ~BaseError() override { --count_; }

  static int count() { return count_; }

  bool HasMenuItem() override { return false; }
  int MenuItemCommandID() override {
    ADD_FAILURE();
    return 0;
  }
  std::u16string MenuItemLabel() override {
    ADD_FAILURE();
    return std::u16string();
  }
  void ExecuteMenuItem(Browser* browser) override { ADD_FAILURE(); }

  bool HasBubbleView() override { return false; }
  bool HasShownBubbleView() override { return false; }
  void ShowBubbleView(Browser* browser) override { ADD_FAILURE(); }
  GlobalErrorBubbleViewBase* GetBubbleView() override { return nullptr; }

 private:
  // This tracks the number BaseError objects that are currently instantiated.
  static int count_;
};

int BaseError::count_ = 0;

// A simple error that only has a menu item.
class MenuError : public BaseError {
 public:
  explicit MenuError(int command_id, Severity severity)
      : command_id_(command_id),
        severity_(severity) {
  }

  MenuError(const MenuError&) = delete;
  MenuError& operator=(const MenuError&) = delete;

  Severity GetSeverity() override { return severity_; }

  bool HasMenuItem() override { return true; }
  int MenuItemCommandID() override { return command_id_; }
  std::u16string MenuItemLabel() override { return std::u16string(); }
  void ExecuteMenuItem(Browser* browser) override {}

 private:
  int command_id_;
  Severity severity_;
};

} // namespace

// Test adding errors to the global error service.
TEST(GlobalErrorServiceTest, AddError) {
  auto service = std::make_unique<GlobalErrorService>();
  EXPECT_EQ(0u, service->errors().size());

  BaseError* error1 = new BaseError;
  service->AddGlobalError(base::WrapUnique(error1));
  EXPECT_EQ(1u, service->errors().size());
  EXPECT_EQ(error1, service->errors()[0]);

  BaseError* error2 = new BaseError;
  service->AddGlobalError(base::WrapUnique(error2));
  EXPECT_EQ(2u, service->errors().size());
  EXPECT_EQ(error1, service->errors()[0]);
  EXPECT_EQ(error2, service->errors()[1]);

  // Ensure that deleting the service deletes the error objects.
  EXPECT_EQ(2, BaseError::count());
  service.reset();
  EXPECT_EQ(0, BaseError::count());
}

// Test removing errors from the global error service.
TEST(GlobalErrorServiceTest, RemoveError) {
  auto service = std::make_unique<GlobalErrorService>();
  BaseError error1;
  service->AddUnownedGlobalError(&error1);
  BaseError error2;
  service->AddUnownedGlobalError(&error2);

  EXPECT_EQ(2u, service->errors().size());
  service->RemoveUnownedGlobalError(&error1);
  EXPECT_EQ(1u, service->errors().size());
  EXPECT_EQ(&error2, service->errors()[0]);
  service->RemoveUnownedGlobalError(&error2);
  EXPECT_EQ(0u, service->errors().size());

  // Ensure that deleting the service does not delete the error objects.
  //
  // NB: If the service _does_ delete the error objects, then it called the
  // delete operator on a stack-allocated object, which is undefined behavior,
  // which we can't really use to prove anything. :(
  EXPECT_EQ(2, BaseError::count());
  service.reset();
  EXPECT_EQ(2, BaseError::count());
}

// Test finding errors by their menu item command ID.
TEST(GlobalErrorServiceTest, GetMenuItem) {
  MenuError* error1 = new MenuError(1, GlobalError::SEVERITY_LOW);
  MenuError* error2 = new MenuError(2, GlobalError::SEVERITY_MEDIUM);
  MenuError* error3 = new MenuError(3, GlobalError::SEVERITY_HIGH);

  GlobalErrorService service;
  service.AddGlobalError(base::WrapUnique(error1));
  service.AddGlobalError(base::WrapUnique(error2));
  service.AddGlobalError(base::WrapUnique(error3));

  EXPECT_EQ(error2, service.GetGlobalErrorByMenuItemCommandID(2));
  EXPECT_EQ(error3, service.GetGlobalErrorByMenuItemCommandID(3));
  EXPECT_EQ(nullptr, service.GetGlobalErrorByMenuItemCommandID(4));
}

// Test getting the error with the highest severity.
TEST(GlobalErrorServiceTest, HighestSeverity) {
  MenuError* error1 = new MenuError(1, GlobalError::SEVERITY_LOW);
  MenuError* error2 = new MenuError(2, GlobalError::SEVERITY_MEDIUM);
  MenuError* error3 = new MenuError(3, GlobalError::SEVERITY_HIGH);

  GlobalErrorService service;
  EXPECT_EQ(nullptr, service.GetHighestSeverityGlobalErrorWithAppMenuItem());

  service.AddGlobalError(base::WrapUnique(error1));
  EXPECT_EQ(error1, service.GetHighestSeverityGlobalErrorWithAppMenuItem());

  service.AddGlobalError(base::WrapUnique(error2));
  EXPECT_EQ(error2, service.GetHighestSeverityGlobalErrorWithAppMenuItem());

  service.AddGlobalError(base::WrapUnique(error3));
  EXPECT_EQ(error3, service.GetHighestSeverityGlobalErrorWithAppMenuItem());

  // Remove the highest-severity error.
  service.RemoveGlobalError(error3);

  // Now error2 should be the next highest severity error.
  EXPECT_EQ(error2, service.GetHighestSeverityGlobalErrorWithAppMenuItem());
}
