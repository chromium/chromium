// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_store.h"

#include <unordered_set>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/templates/note_template.h"
#include "components/content_creation/notes/core/templates/template_types.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_creation {

class TemplateStoreTest : public testing::Test {
  void SetUp() override {
    template_store_ = std::make_unique<TemplateStore>(
        &testing_pref_service_, test_url_loader_factory());
  }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

  void ValidateTemplates(const std::vector<NoteTemplate>& note_templates) {
    std::unordered_set<NoteTemplateIds> ids_set;
    for (const NoteTemplate& note_template : note_templates) {
      EXPECT_LT(NoteTemplateIds::kUnknown, note_template.id());
      EXPECT_GE(NoteTemplateIds::kMaxValue, note_template.id());
      EXPECT_FALSE(note_template.localized_name().empty());
      EXPECT_FALSE(note_template.text_style().font_name().empty());

      // There should be no duplicated IDs.
      EXPECT_TRUE(ids_set.find(note_template.id()) == ids_set.end());
      ids_set.insert(note_template.id());
    }
  }

  // Have to use TaskEnvironment since the TemplateStore posts tasks to the
  // thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple testing_pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TemplateStore> template_store_;
};

// Tests that the store does return templates, and also validates the
// templates' information.
TEST_F(TemplateStoreTest, GetTemplatesSuccessWithDefaultTemplates) {
  scoped_feature_list_.InitWithFeatures({kWebNotesStylizeEnabled},
                                        {kWebNotesDynamicTemplates});
  base::RunLoop run_loop;

  template_store_->GetTemplates(base::BindLambdaForTesting(
      [&run_loop, this](std::vector<NoteTemplate> templates) {
        EXPECT_EQ(10U, templates.size());

        ValidateTemplates(templates);

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(TemplateStoreTest, GetTemplatesSuccessWithDynamicTemplates) {
  scoped_feature_list_.InitWithFeatures(
      {kWebNotesStylizeEnabled, kWebNotesDynamicTemplates}, {});
  base::RunLoop run_loop;

  template_store_->GetTemplates(base::BindLambdaForTesting(
      [&run_loop, this](std::vector<NoteTemplate> templates) {
        EXPECT_EQ(10U, templates.size());

        ValidateTemplates(templates);

        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace content_creation
