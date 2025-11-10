// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/bind_weak_ptr_for_export_method.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/memory/weak_ptr.h"
#include "components/dbus/utils/export_method.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus_utils {

namespace {

class TestClass {
 public:
  ExportMethodResult<bool> TestMethod(std::string s, int32_t i) {
    return std::make_tuple(true);
  }
  base::WeakPtr<TestClass> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestClass> weak_ptr_factory_{this};
};

}  // namespace

TEST(BindWeakPtrForExportMethodTest, Alive) {
  TestClass obj;
  auto callback =
      BindWeakPtrForExportMethod(&TestClass::TestMethod, obj.GetWeakPtr());
  auto result = callback.Run("hello", 42);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(std::get<0>(*result));
}

TEST(BindWeakPtrForExportMethodTest, Destroyed) {
  std::unique_ptr<TestClass> obj = std::make_unique<TestClass>();
  auto callback =
      BindWeakPtrForExportMethod(&TestClass::TestMethod, obj->GetWeakPtr());
  obj.reset();
  auto result = callback.Run("hello", 42);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().name, DBUS_ERROR_FAILED);
  EXPECT_EQ(result.error().message, "Object destroyed");
}

}  // namespace dbus_utils
