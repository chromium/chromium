// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/databinding/binding.h"

#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {

struct TestModel {
  bool value;
};

struct TestView {
  bool value;
  std::unique_ptr<Binding<bool>> binding;
};

}  // namespace

TEST(Binding, BoundBool) {
  TestModel a;
  a.value = true;

  TestView b;
  b.value = false;

  b.binding =
      VR_BIND_FIELD(bool, TestModel, &a, model->value, TestView, &b, value);

  EXPECT_NE(a.value, b.value);
  b.binding->Update();

  EXPECT_EQ(true, a.value);
  EXPECT_EQ(true, b.value);

  a.value = false;
  EXPECT_EQ(true, b.value);

  b.binding->Update();
  EXPECT_EQ(false, a.value);
  EXPECT_EQ(false, b.value);

  b.value = true;
  // Since this is a one way binding, Update will not detect a change in value
  // of b. Since a's value has not changed, a new value should not be pushed
  // to b.
  b.binding->Update();
  EXPECT_EQ(false, a.value);
  EXPECT_EQ(true, b.value);
}

TEST(Binding, HistoricBinding) {
  TestModel a;
  a.value = true;

  TestView b;
  b.value = false;

  b.binding = std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA([](TestModel* m) { return m->value; },
                     base::Unretained(&a)),
      VR_BIND_LAMBDA(
          [](TestView* v, const std::optional<bool>& last_value,
             const bool& value) {
            if (last_value)
              v->value = value;
          },
          base::Unretained(&b)));

  EXPECT_NE(a.value, b.value);
  b.binding->Update();

  EXPECT_EQ(true, a.value);
  EXPECT_EQ(false, b.value);

  a.value = false;
  b.binding->Update();
  EXPECT_EQ(false, b.value);

  a.value = true;
  b.binding->Update();
  EXPECT_EQ(true, b.value);
}

}  // namespace vr
