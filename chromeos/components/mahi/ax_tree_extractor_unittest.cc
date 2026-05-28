// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/ax_tree_extractor.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_tree_update.h"

namespace mahi {

class AXTreeExtractorTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AXTreeExtractorTest, ExtractContentFromFailedUpdateReturnsNull) {
  AXTreeExtractor extractor;

  mojom::ExtractionRequestPtr request = mojom::ExtractionRequest::New();

  // Construct an update that is guaranteed to fail Unserialize.
  // Root (1) has child 0, but node 0 is not defined in the update.
  ui::AXTreeUpdate update1;
  update1.root_id = 1;
  update1.nodes.resize(1);
  update1.nodes[0].id = 1;
  update1.nodes[0].role = ax::mojom::Role::kRootWebArea;
  update1.nodes[0].child_ids.push_back(0);  // Invalid ID 0

  request->updates = std::vector<ui::AXTreeUpdate>{update1};

  mojom::ExtractionMethodsPtr extraction_methods =
      mojom::ExtractionMethods::New();
  extraction_methods->use_algorithm = true;
  request->extraction_methods = std::move(extraction_methods);

#if AX_FAIL_FAST_BUILD()
  EXPECT_DEATH_IF_SUPPORTED(
      extractor.ExtractContent(std::move(request), base::DoNothing()),
      "Child ID is invalid.");
#else
  base::RunLoop run_loop;
  extractor.ExtractContent(
      std::move(request),
      base::BindOnce([](mojom::ExtractionResponsePtr response) {
        // We expect the response to be nullptr because Unserialize failed
        // and AXTreeExtractor aborted immediately.
        EXPECT_TRUE(response.is_null());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
#endif
}

}  // namespace mahi
