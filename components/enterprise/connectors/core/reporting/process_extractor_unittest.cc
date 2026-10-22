// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting/process_extractor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

TEST(ProcessExtractorTest, ProcessTreeAndNodeConstruction) {
  ProcessTree tree;
  EXPECT_TRUE(tree.empty());

  tree.process_nodes.emplace_back(1234, 567, "/usr/local/bin/node",
                                  "/Users/<USER>/agent.js");
  EXPECT_FALSE(tree.empty());
  EXPECT_EQ(tree.process_nodes[0].process_id,
            static_cast<base::ProcessId>(1234));
  EXPECT_EQ(tree.process_nodes[0].parent_process_id,
            static_cast<base::ProcessId>(567));
  EXPECT_EQ(tree.process_nodes[0].executable_path, "/usr/local/bin/node");
  EXPECT_EQ(tree.process_nodes[0].script_path, "/Users/<USER>/agent.js");
}

}  // namespace

}  // namespace enterprise_connectors
