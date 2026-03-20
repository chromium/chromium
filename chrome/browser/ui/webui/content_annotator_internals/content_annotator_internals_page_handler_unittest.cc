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
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_annotator_internals {

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
                      accessibility_annotator::AccessibilityAnnotatorBackend>(
                      version_info::Channel::UNKNOWN,
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

  backend->SetContentAnnotationsCacheData(GURL("https://example.com"), "Title",
                                          "{\"key\": \"value\"}");

  base::RunLoop run_loop;
  handler()->GetAnnotatedContent(
      base::BindLambdaForTesting([&](base::Value content) {
        ASSERT_TRUE(content.is_list());
        const base::ListValue& list = content.GetList();
        ASSERT_EQ(list.size(), 1u);
        const base::DictValue& entry = list[0].GetDict();
        EXPECT_EQ(*entry.FindString("url"), "https://example.com/");
        EXPECT_EQ(*entry.FindString("title"), "Title");
        const base::DictValue* annotations = entry.FindDict("annotations");
        ASSERT_TRUE(annotations);
        EXPECT_EQ(*annotations->FindString("key"), "value");
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace content_annotator_internals
