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
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/logging/accessibility_annotator_internals.mojom.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/history/core/browser/history_types.h"
#include "components/os_crypt/async/browser/test_utils.h"
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
              OnContentAnnotationsChanged,
              (base::Value content),
              (override));

  MOCK_METHOD(void, OnContentAnnotationsCleared, (), (override));

 private:
  mojo::Receiver<accessibility_annotator_internals::mojom::Page> receiver_{
      this};
};

class ContentAnnotatorInternalsPageHandlerTest
    : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          accessibility_annotator::features::
              kAccessibilityAnnotatorDatabaseStorage);
    }

    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);

    // Set up the AccessibilityAnnotatorBackendFactory to use a real backend
    // with an in-memory store.
    AccessibilityAnnotatorBackendFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(
                [](os_crypt_async::OSCryptAsync* os_crypt_async,
                   base::FilePath path, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      accessibility_annotator::
                          AccessibilityAnnotatorBackendImpl>(
                      /*history_service=*/nullptr, os_crypt_async,
                      syncer::DataTypeStoreTestUtil::
                          FactoryForInMemoryStoreForTest(),
                      path.Append(
                          FILE_PATH_LITERAL("AccessibilityAnnotatorDatabase")));
                },
                os_crypt_async_.get(), temp_dir_.GetPath()));

    handler_ = std::make_unique<ContentAnnotatorInternalsPageHandler>(
        mojo::PendingReceiver<
            accessibility_annotator_internals::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), &profile_);
  }

  void SetContentAnnotationsData(
      history::VisitID visit_id,
      accessibility_annotator::AccessibilityAnnotatorBackend::
          ContentAnnotationsData data) {
    accessibility_annotator::AccessibilityAnnotatorBackend* backend =
        AccessibilityAnnotatorBackendFactory::GetForProfile(profile());
    ASSERT_TRUE(backend);
    if (GetParam()) {
      base::test::TestFuture<bool> future;
      backend->AddContentAnnotation(visit_id, std::move(data),
                                    future.GetCallback());
      ASSERT_TRUE(future.Get());
    } else {
      backend->SetContentAnnotationsCacheData(visit_id, std::move(data));
    }
  }

  TestingProfile* profile() { return &profile_; }
  ContentAnnotatorInternalsPageHandler* handler() { return handler_.get(); }
  base::ScopedTempDir& temp_dir() { return temp_dir_; }
  MockPage& mock_page() { return mock_page_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  TestingProfile profile_;
  MockPage mock_page_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::unique_ptr<ContentAnnotatorInternalsPageHandler> handler_;
};

TEST_P(ContentAnnotatorInternalsPageHandlerTest, GetAnnotatedContentEmpty) {
  base::RunLoop run_loop;
  handler()->GetAnnotatedContent(
      base::BindLambdaForTesting([&](base::Value content) {
        ASSERT_TRUE(content.is_list());
        EXPECT_TRUE(content.GetList().empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_P(ContentAnnotatorInternalsPageHandlerTest, GetAnnotatedContentWithData) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  base::test::ScopedRestoreDefaultTimezone timezone("UTC");

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

  SetContentAnnotationsData(static_cast<history::VisitID>(123),
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

TEST_P(ContentAnnotatorInternalsPageHandlerTest, ClearContentAnnotations) {
  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data;
  data.page_title = "Title";
  data.url = GURL("https://example.com");
  SetContentAnnotationsData(static_cast<history::VisitID>(1), std::move(data));

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

  // Clear data.
  {
    base::RunLoop run_loop;
    handler()->ClearAnnotatedContent(
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Verify data is empty.
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

TEST_P(ContentAnnotatorInternalsPageHandlerTest, DeleteAnnotatedContent) {
  // Add two entries.
  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data1;
  data1.page_title = "Title 1";
  data1.url = GURL("https://example.com/1");
  SetContentAnnotationsData(static_cast<history::VisitID>(1), std::move(data1));

  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data2;
  data2.page_title = "Title 2";
  data2.url = GURL("https://example.com/2");
  SetContentAnnotationsData(static_cast<history::VisitID>(2), std::move(data2));

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

TEST_P(ContentAnnotatorInternalsPageHandlerTest,
       OnContentAnnotationsChangedPushesToUI) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  base::test::ScopedRestoreDefaultTimezone timezone("UTC");

  // Add first annotation and verify the UI is notified.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page(), OnContentAnnotationsChanged(Property(
                                 &base::Value::GetList,
                                 ElementsAre(DictionaryHasValues(
                                     base::DictValue()
                                         .Set("url", "https://example.com/1")
                                         .Set("title", "Title 1")
                                         .Set("visit_id", "123"))))))
        .WillOnce([&](const base::Value& content) { run_loop.Quit(); });

    accessibility_annotator::AccessibilityAnnotatorBackend::
        ContentAnnotationsData data;
    data.page_title = "Title 1";
    data.url = GURL("https://example.com/1");
    SetContentAnnotationsData(static_cast<history::VisitID>(123),
                              std::move(data));
    run_loop.Run();
  }

  // Add second annotation and verify the UI is notified with both.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_page(),
        OnContentAnnotationsChanged(Property(
            &base::Value::GetList,
            UnorderedElementsAre(
                DictionaryHasValues(base::DictValue()
                                        .Set("visit_id", "123")
                                        .Set("url", "https://example.com/1")
                                        .Set("title", "Title 1")),
                DictionaryHasValues(base::DictValue()
                                        .Set("visit_id", "456")
                                        .Set("url", "https://example.com/2")
                                        .Set("title", "Title 2"))))))
        .WillOnce([&](const base::Value& content) { run_loop.Quit(); });

    accessibility_annotator::AccessibilityAnnotatorBackend::
        ContentAnnotationsData data;
    data.page_title = "Title 2";
    data.url = GURL("https://example.com/2");
    SetContentAnnotationsData(static_cast<history::VisitID>(456),
                              std::move(data));
    run_loop.Run();
  }

  // Delete one annotation and verify that the UI is notified with the
  // remaining one.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page(), OnContentAnnotationsChanged(Property(
                                 &base::Value::GetList,
                                 ElementsAre(DictionaryHasValues(
                                     base::DictValue()
                                         .Set("url", "https://example.com/2")
                                         .Set("title", "Title 2")
                                         .Set("visit_id", "456"))))))
        .WillOnce([&](const base::Value& content) { run_loop.Quit(); });

    base::test::TestFuture<bool> delete_future;
    handler()->DeleteAnnotatedContent({123}, delete_future.GetCallback());
    ASSERT_TRUE(delete_future.Get());
    run_loop.Run();
  }

  // Delete the last annotation and verify that the UI is notified with an
  // empty list.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page(), OnContentAnnotationsChanged(Property(
                                 &base::Value::GetList, testing::IsEmpty())))
        .WillOnce([&](const base::Value& content) { run_loop.Quit(); });

    base::test::TestFuture<bool> delete_future;
    handler()->DeleteAnnotatedContent({456}, delete_future.GetCallback());
    ASSERT_TRUE(delete_future.Get());
    run_loop.Run();
  }
}

TEST_P(ContentAnnotatorInternalsPageHandlerTest,
       OnContentAnnotationsClearedPushesToUI) {
  accessibility_annotator::AccessibilityAnnotatorBackend::ContentAnnotationsData
      data;
  data.page_title = "Title";
  data.url = GURL("https://example.com");

  // Add data and verify that the UI is notified.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page(), OnContentAnnotationsChanged(testing::_))
        .WillOnce([&](const base::Value& content) { run_loop.Quit(); });
    SetContentAnnotationsData(static_cast<history::VisitID>(456),
                              std::move(data));
    run_loop.Run();
  }

  // Expectation for clearing.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page(), OnContentAnnotationsCleared()).WillOnce([&]() {
      run_loop.Quit();
    });

    base::test::TestFuture<bool> clear_future;
    handler()->ClearAnnotatedContent(clear_future.GetCallback());
    ASSERT_TRUE(clear_future.Get());
    run_loop.Run();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContentAnnotatorInternalsPageHandlerTest,
                         testing::Bool());

}  // namespace
}  // namespace content_annotator_internals
