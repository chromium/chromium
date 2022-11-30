// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/note_service.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/server/save_note_response.h"
#include "components/content_creation/notes/core/templates/note_template.h"
#include "components/content_creation/notes/core/test/mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content_creation {

class NoteServiceTest : public testing::Test {
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kWebNotesStylizeEnabled);

    auto mock_template_store = std::make_unique<test::MockTemplateStore>();
    mock_template_store_ = mock_template_store.get();

    auto mock_notes_repository = std::make_unique<test::MockNotesRepository>();
    mock_notes_repository_ = mock_notes_repository.get();

    note_service_ = std::make_unique<NoteService>(
        std::move(mock_template_store), std::move(mock_notes_repository));
  }

 protected:
  // Have to use TaskEnvironment since the TemplateStore posts tasks to the
  // thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NoteService> note_service_;
  raw_ptr<test::MockTemplateStore> mock_template_store_;
  raw_ptr<test::MockNotesRepository> mock_notes_repository_;
};

TEST_F(NoteServiceTest, GetTemplatesSuccess_Empty) {
  EXPECT_CALL(*mock_template_store_, GetTemplates(_)).Times(1);

  note_service_->GetTemplates(base::BindLambdaForTesting(
      [](std::vector<NoteTemplate> templates) { /* No-op */ }));
}

TEST_F(NoteServiceTest, PublishNoteSuccess_Empty) {
  EXPECT_CALL(*mock_notes_repository_, PublishNote(_, _)).Times(1);

  NoteData data("", "", GURL(), "");
  note_service_->PublishNote(
      data,
      base::BindLambdaForTesting([](std::string response) { /* No-op */ }));
}

}  // namespace content_creation
