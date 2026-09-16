// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/render_process_host_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(RenderProcessHostIdTest, InvalidValues) {
  RenderProcessHostId default_id;
  EXPECT_TRUE(default_id->is_null());
  EXPECT_FALSE(*default_id);

  RenderProcessHostId invalid_id{content::ChildProcessId{}};
  EXPECT_TRUE(invalid_id->is_null());
  EXPECT_FALSE(*invalid_id);

  RenderProcessHostId zero_id(content::ChildProcessId(0));
  EXPECT_TRUE(zero_id->is_null());
  EXPECT_FALSE(*zero_id);

  EXPECT_EQ(default_id, invalid_id);
  EXPECT_NE(default_id, zero_id);

  RenderProcessHostId valid_id(content::ChildProcessId(1));
  EXPECT_FALSE(valid_id->is_null());
  EXPECT_TRUE(*valid_id);
}

TEST(RenderProcessHostIdTest, Generator) {
  RenderProcessHostId::underlying_type::Generator generator;
  EXPECT_EQ(RenderProcessHostId(generator.GenerateNextId()),
            RenderProcessHostId(content::ChildProcessId(1)));
  EXPECT_EQ(RenderProcessHostId(generator.GenerateNextId()),
            RenderProcessHostId(content::ChildProcessId(2)));
}

}  // namespace performance_manager
