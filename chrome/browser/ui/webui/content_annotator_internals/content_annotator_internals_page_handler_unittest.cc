// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/content_annotator_internals/content_annotator_internals_page_handler.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/logging/accessibility_annotator_internals.mojom.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_annotator_internals {

using ::testing::Eq;
using ::testing::Pointee;

namespace {

class MockPage : public accessibility_annotator_internals::mojom::Page {
 public:
  mojo::PendingRemote<accessibility_annotator_internals::mojom::Page>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<accessibility_annotator_internals::mojom::Page> receiver_{
      this};
};

class ContentAnnotatorInternalsPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Set up the AccessibilityAnnotatorBackendFactory to use a real backend
    // with an in-memory store.
    AccessibilityAnnotatorBackendFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(
                [](base::FilePath path, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      accessibility_annotator::
                          AccessibilityAnnotatorBackendImpl>(
                      /*history_service=*/nullptr,
                      syncer::DataTypeStoreTestUtil::
                          FactoryForInMemoryStoreForTest(),
                      path.Append(
                          FILE_PATH_LITERAL("AccessibilityAnnotatorDatabase")));
                },
                temp_dir_.GetPath()));

    handler_ = std::make_unique<ContentAnnotatorInternalsPageHandler>(
        mojo::PendingReceiver<
            accessibility_annotator_internals::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), &profile_);
  }

  TestingProfile* profile() { return &profile_; }
  ContentAnnotatorInternalsPageHandler* handler() { return handler_.get(); }
  base::ScopedTempDir& temp_dir() { return temp_dir_; }
  MockPage& mock_page() { return mock_page_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  TestingProfile profile_;
  MockPage mock_page_;
  std::unique_ptr<ContentAnnotatorInternalsPageHandler> handler_;
};

TEST_F(ContentAnnotatorInternalsPageHandlerTest, GetAnnotatedContentEmpty) {
  base::RunLoop run_loop;
  handler()->GetAnnotatedContent(
      base::BindLambdaForTesting([&](base::Value content) {
        ASSERT_TRUE(content.is_list());
        EXPECT_TRUE(content.GetList().empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ContentAnnotatorInternalsPageHandlerTest, GetAnnotatedContentWithData) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile());
  ASSERT_TRUE(backend);

  base::DictValue annotations;
  annotations.Set("key", "value");
  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "test category");

  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data;
  data.page_title = "Title";
  data.tab_id = 123;
  data.annotations = std::move(annotations);
  data.classifier_results = std::move(classifier_results);

  backend->SetContentAnnotationsCacheData(GURL("https://example.com"),
                                          std::move(data));

  base::RunLoop run_loop;
  handler()->GetAnnotatedContent(
      base::BindLambdaForTesting([&](base::Value content) {
        ASSERT_TRUE(content.is_list());
        const base::ListValue& list = content.GetList();
        ASSERT_EQ(list.size(), 1u);
        const base::DictValue& entry = list[0].GetDict();
        EXPECT_THAT(entry.FindString("url"), Pointee(Eq("https://example.com/")));
        EXPECT_THAT(entry.FindString("title"), Pointee(Eq("Title")));
        EXPECT_THAT(entry.FindInt("tab_id"), 123);
        const base::DictValue* annotations = entry.FindDict("annotations");
        ASSERT_TRUE(annotations);
        EXPECT_THAT(annotations->FindString("key"), Pointee(Eq("value")));

        const base::DictValue* classifier_results =
            entry.FindDict("classifier_results");
        ASSERT_TRUE(classifier_results);
        EXPECT_THAT(classifier_results->FindString("url_match_result"),
                    Pointee(Eq("test category")));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ContentAnnotatorInternalsPageHandlerTest, ClearContentAnnotationsCache) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile());
  ASSERT_TRUE(backend);

  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data;
  data.page_title = "Title";
  backend->SetContentAnnotationsCacheData(GURL("https://example.com"),
                                          std::move(data));

  // Verify data is present.
  {
    base::RunLoop run_loop;
    handler()->GetAnnotatedContent(
        base::BindLambdaForTesting([&](base::Value content) {
          ASSERT_TRUE(content.is_list());
          EXPECT_EQ(content.GetList().size(), 1u);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Clear cache.
  {
    base::RunLoop run_loop;
    handler()->ClearAnnotatedContent(
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Verify cache is empty.
  {
    base::RunLoop run_loop;
    handler()->GetAnnotatedContent(
        base::BindLambdaForTesting([&](base::Value content) {
          ASSERT_TRUE(content.is_list());
          EXPECT_TRUE(content.GetList().empty());
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  // namespace
}  // namespace content_annotator_internals
