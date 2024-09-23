// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/content_extraction_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/test_ax_tree_update.h"

namespace mahi {

class ContentExtractionServiceTestBase : public testing::Test {
 public:
  void OnContentExtracted(const std::u16string& expected_contents,
                          mojom::ExtractionResponsePtr response) {
    EXPECT_EQ(expected_contents, response->contents);
    EXPECT_EQ(mojom::ResponseStatus::kSuccess, response->status);
  }

  void OnGetContentSize(int expected_size,
                        mojom::ContentSizeResponsePtr response) {
    EXPECT_EQ(expected_size, response->word_count);
    EXPECT_EQ(mojom::ResponseStatus::kSuccess, response->status);
  }

  void TestContentExtraction(const std::string& tree_structure,
                             const std::u16string& expected_contents,
                             int expected_size) {
    mojom::ExtractionRequestPtr request = mojom::ExtractionRequest::New();

    ui::TestAXTreeUpdate update(tree_structure);
    request->snapshot = update;

    mojom::ExtractionMethodsPtr extraction_methods =
        mojom::ExtractionMethods::New();
    extraction_methods->use_algorithm = true;
    extraction_methods->use_screen2x = false;
    request->extraction_methods = std::move(extraction_methods);

    service_->ExtractContent(
        request->Clone(),
        base::BindOnce(&ContentExtractionServiceTestBase::OnContentExtracted,
                       weak_factory_.GetWeakPtr(),
                       std::move(expected_contents)));

    service_->GetContentSize(
        std::move(request),
        base::BindOnce(&ContentExtractionServiceTestBase::OnGetContentSize,
                       weak_factory_.GetWeakPtr(), expected_size));
  }

 protected:
  void SetUp() override {
    service_ = std::make_unique<ContentExtractionService>(
        factory_remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<mojom::ContentExtractionServiceFactory> factory_remote_;
  std::unique_ptr<ContentExtractionService> service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  int receiver_count_;
  base::WeakPtrFactory<ContentExtractionServiceTestBase> weak_factory_{this};
};

TEST_F(ContentExtractionServiceTestBase, BindContentExtractionService) {
  mojo::Remote<mojom::ContentExtractionService> service_remote;
  service_->BindContentExtractionService(
      service_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(service_remote.is_bound());
}

struct TestCase {
  const char* test_name;
  std::string tree_structure;
  std::u16string expected_contents;
  int expected_size;
};

class ContentExtractionServiceTest
    : public ContentExtractionServiceTestBase,
      public ::testing::WithParamInterface<TestCase> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<TestCase> param_info) {
    return param_info.param.test_name;
  }
};

// Each test case of the object below is used by the parameterized test.
// A dividing line is included between test cases for better readability.
// Note: `ui::TestAXTreeUpdate` does not allow whitespace within property
//        values, so "-" is used instead.
const TestCase kTreeExtractionTestCases[] = {
    {
        "simple_page",
        R"HTML(
    ++1 kRootWebArea name="document"
    ++++2 kMain
    ++++++3 kParagraph
    ++++++++4 kStaticText name="some-text"
  )HTML",
        u"some-text",
        2,
    },
    /* ----------------------- */
    {
        "simple_page_with_heading",
        R"HTML(
    ++1 kRootWebArea name="document"
    ++++2 kMain
    ++++++3 kHeading name="header"
    ++++++++7 kStaticText name="heading-1"
    ++++++4 kParagraph
    ++++++++8 kStaticText name="some-text"
    ++++++5 kParagraph
    ++++++++9 kStaticText name="some-other-text"
    ++++++6 kGenericContainer
    ++++++++10 kHeading name="header"
    ++++++++++11 kStaticText name="heading-2"
  )HTML",
        u"heading-1\n\nsome-text\n\nsome-other-text\n\nheading-2",
        9,
    },
    /* ----------------------- */
    {
        "simple_page_with_article",
        R"HTML(
    ++1 kRootWebArea name="document"
    ++++2 kMain
    ++++++3 kParagraph
    ++++++++6 kStaticText name="some-text"
    ++++++4 kArticle
    ++++++++7 kParagraph
    ++++++++++9 kStaticText name="article-text"
    ++++++5 kArticle
    ++++++++8 kParagraph
    ++++++++++10 kStaticText name="some-other-text"
  )HTML",
        u"some-text\n\narticle-text\n\nsome-other-text",
        7,
    },
    /* ----------------------- */
    {
        "simple_page_with_article_hierarchy",
        R"HTML(
    ++1 kRootWebArea name="document"
    ++++2 kMain
    ++++++3 kArticle
    ++++++++6 kParagraph
    ++++++++++9 kStaticText name="article-text"
    ++++++4 kParagraph
    ++++++++7 kStaticText name="some-text"
    ++++++5 kArticle
    ++++++++8 kParagraph
    ++++++++++10 kStaticText name="some-other-text"
  )HTML",
        u"article-text\n\nsome-text\n\nsome-other-text",
        7,
    },
    /* ----------------------- */
    {
        "simple_page_unsupported_roles",
        R"HTML(
    ++1 kRootWebArea name="document"
    ++++2 kGenericContainer
    ++++++3 kMain
    ++++++++4 kBanner
    ++++++++++10 kStaticText name="banner"
    ++++++++5 kNavigation
    ++++++++++11 kStaticText name="navigation"
    ++++++++6 kImage
    ++++++++7 kButton
    ++++++++++12 kStaticText name="button"
    ++++++++8 kContentInfo
    ++++++++++13 kStaticText name="content-info"
    ++++++++9 kFooter
    ++++++++++14 kStaticText name="footer"
  )HTML",
        u"",
        0,
    },
    /* ----------------------- */
};

TEST_P(ContentExtractionServiceTest, ContentExtraction) {
  TestCase param = GetParam();
  TestContentExtraction(param.tree_structure, param.expected_contents,
                        param.expected_size);
}

INSTANTIATE_TEST_SUITE_P(/* prefix */,
                         ContentExtractionServiceTest,
                         ::testing::ValuesIn(kTreeExtractionTestCases),
                         ContentExtractionServiceTest::ParamInfoToString);

}  // namespace mahi
