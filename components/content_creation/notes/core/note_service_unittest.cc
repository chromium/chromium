// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/note_service.h"

#include <vector>

#include "base/test/bind.h"
#include "components/content_creation/notes/core/notes_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_creation {

class NoteServiceTest : public testing::Test {
 protected:
  NoteService note_service_;
};

TEST_F(NoteServiceTest, GetTemplatesSuccess) {
  bool invoked = false;

  note_service_.GetTemplates(base::BindLambdaForTesting(
      [&invoked](std::vector<NoteTemplate> templates) {
        EXPECT_EQ(0U, templates.size());
        invoked = true;
      }));

  EXPECT_TRUE(invoked);
}

}  // namespace content_creation
