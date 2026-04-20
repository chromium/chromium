// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/content_annotator_internals/content_annotator_internals_page_handler.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/logging/accessibility_annotator_internals.mojom.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_annotator_internals {

using ::base::test::DictionaryHasValues;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;

namespace {

class MockPage : public accessibility_annotator_internals::mojom::Page {
 public:
  mojo::PendingRemote<accessibility_annotator_internals::mojom::Page>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnContentAnnotationsAdded,
              (base::Value content),
              (override));

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
                      /*os_crypt_async=*/nullptr,
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
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  base::test::ScopedRestoreDefaultTimezone timezone("UTC");
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile());
  ASSERT_TRUE(backend);

  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "test category");

  base::DictValue expected_classifier = classifier_results.Clone();
  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2026-04-10 10:00:00 UTC", &now));

  optimization_guide::proto::ContentAnnotation content_annotation;
  content_annotation.set_description("Test annotation description");
  content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* order = content_annotation.mutable_structured_data()->add_orders();
  order->set_id("order_123");

  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data;
  data.page_title = "Title";
  data.tab_id = 123;
  data.url = GURL("https://example.com");
  data.navigation_timestamp = now;
  data.content_annotation = std::move(content_annotation);
  data.classifier_results = std::move(classifier_results);

  backend->SetContentAnnotationsCacheData(static_cast<history::VisitID>(123),
                                          std::move(data));

  base::RunLoop run_loop;
  handler()->GetAnnotatedContent(
      base::BindLambdaForTesting([&](base::Value content) {
        ASSERT_TRUE(content.is_list());
        const base::ListValue& list = content.GetList();
        ASSERT_EQ(list.size(), 1u);
        const base::DictValue& entry = list[0].GetDict();
        EXPECT_THAT(
            entry,
            DictionaryHasValues(
                base::DictValue()
                    .Set("url", "https://example.com/")
                    .Set("title", "Title")
                    .Set("tab_id", 123)
                    .Set("visit_id", "123")
                    .Set("navigation_timestamp",
                         "4/10/26, 10:00:00\xe2\x80\xaf"
                         "AM")
                    .Set("classifier_results", std::move(expected_classifier))
                    .Set(
                        "content_annotation",
                        base::DictValue()
                            .Set("description", "Test annotation description")
                            .Set("status", "CONFIRMED")
                            .Set("structured_data",
                                 base::DictValue().Set(
                                     "orders", base::ListValue().Append(
                                                   base::DictValue().Set(
                                                       "id", "order_123")))))));
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
  data.url = GURL("https://example.com");
  backend->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
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

TEST_F(ContentAnnotatorInternalsPageHandlerTest, DeleteAnnotatedContent) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile());
  ASSERT_TRUE(backend);

  // Add two entries.
  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data1;
  data1.page_title = "Title 1";
  data1.url = GURL("https://example.com/1");
  backend->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                          std::move(data1));

  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data2;
  data2.page_title = "Title 2";
  data2.url = GURL("https://example.com/2");
  backend->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                          std::move(data2));

  // Verify data is present.
  {
    base::RunLoop run_loop;
    handler()->GetAnnotatedContent(
        base::BindLambdaForTesting([&](base::Value content) {
          ASSERT_TRUE(content.is_list());
          EXPECT_EQ(content.GetList().size(), 2u);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Delete one entry.
  {
    base::RunLoop run_loop;
    std::vector<int64_t> visit_ids = {1};
    handler()->DeleteAnnotatedContent(
        visit_ids, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Verify only one entry remains.
  {
    base::RunLoop run_loop;
    handler()->GetAnnotatedContent(
        base::BindLambdaForTesting([&](base::Value content) {
          ASSERT_TRUE(content.is_list());
          const base::ListValue& list = content.GetList();
          ASSERT_EQ(list.size(), 1u);
          const base::DictValue& entry = list[0].GetDict();
          EXPECT_THAT(entry.FindString("url"),
                      Pointee(Eq("https://example.com/2")));
          EXPECT_THAT(entry.FindString("title"), Pointee(Eq("Title 2")));
          EXPECT_THAT(entry.FindString("visit_id"), Pointee(Eq("2")));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(ContentAnnotatorInternalsPageHandlerTest,
       OnContentAnnotationsAddedPushesToUI) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  base::test::ScopedRestoreDefaultTimezone timezone("UTC");

  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile());
  ASSERT_TRUE(backend);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page(), OnContentAnnotationsAdded(Property(
                               &base::Value::GetList,
                               ElementsAre(DictionaryHasValues(
                                   base::DictValue()
                                       .Set("url", "https://example.com/")
                                       .Set("title", "Title")
                                       .Set("visit_id", "123"))))))
      .WillOnce([&](const base::Value& content) { run_loop.Quit(); });

  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data;
  data.page_title = "Title";
  data.url = GURL("https://example.com");
  backend->SetContentAnnotationsCacheData(static_cast<history::VisitID>(123),
                                          std::move(data));
  run_loop.Run();
}

}  // namespace
}  // namespace content_annotator_internals
