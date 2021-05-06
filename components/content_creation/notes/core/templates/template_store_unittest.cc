// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_store.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/content_creation/notes/core/templates/note_template.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_creation {

class TemplateStoreTest : public testing::Test {
  void SetUp() override { template_store_ = std::make_unique<TemplateStore>(); }

 protected:
  // Have to use TaskEnvironment since the TemplateStore posts tasks to the
  // thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TemplateStore> template_store_;
};

TEST_F(TemplateStoreTest, GetTemplatesSuccess) {
  base::RunLoop run_loop;

  template_store_->GetTemplates(base::BindLambdaForTesting(
      [&run_loop](std::vector<NoteTemplate> templates) {
        EXPECT_EQ(1U, templates.size());
        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace content_creation
