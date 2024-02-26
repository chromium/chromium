// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/inspect.h"

#include <lib/inspect/cpp/hierarchy.h>
#include <lib/inspect/cpp/reader.h>

#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia_component_support {

namespace {

const char kVersion[] = "version";
const char kLastChange[] = "last_change_revision";

}  // namespace

TEST(InspectTest, PublishVersionInfoToInspect) {
  inspect::Inspector inspector;

  fuchsia_component_support::PublishVersionInfoToInspect(&inspector.GetRoot());

  // Parse the data as an inspect::Hierarchy.
  auto h = inspect::ReadFromVmo(inspector.DuplicateVmo());
  inspect::Hierarchy hierarchy = std::move(h.value());

  auto* property =
      hierarchy.node().get_property<inspect::StringPropertyValue>(kVersion);
  EXPECT_EQ(property->value(), version_info::GetVersionNumber());
  property =
      hierarchy.node().get_property<inspect::StringPropertyValue>(kLastChange);
  EXPECT_EQ(property->value(), version_info::GetLastChange());
}

}  // namespace fuchsia_component_support
