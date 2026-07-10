// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"

namespace optimization_guide {

namespace {

class MockEligibilityAPI {
 public:
  MOCK_METHOD(bool,
              IsPageContextEligibleWithAccount,
              (const std::string&,
               const std::string&,
               const std::string&,
               const std::vector<FrameMetadata>&));
  MOCK_METHOD(PageEligibilityResult,
              CheckPageEligibility,
              (const std::vector<FrameUrl>&));
};

MockEligibilityAPI* g_mock_api = nullptr;

bool MockIsPageContextEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<FrameMetadata>& frame_metadata) {
  return true;
}

bool MockIsPageContextEligibleWithAccount(
    const std::string& host,
    const std::string& path,
    const std::string& account,
    const std::vector<FrameMetadata>& frame_metadata) {
  if (g_mock_api) {
    return g_mock_api->IsPageContextEligibleWithAccount(host, path, account,
                                                        frame_metadata);
  }
  return true;
}

bool MockShouldReextractPageContext(
    const std::string& host,
    const std::string& path,
    const std::vector<std::string>& updated_meta_tags) {
  return false;
}

PageEligibilityResult MockCheckPageEligibility(
    const std::vector<FrameUrl>& frames) {
  if (g_mock_api) {
    return g_mock_api->CheckPageEligibility(frames);
  }
  return PageEligibilityResult{
      .status = PageEligibility::kEligible,
      .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
}

PageContextEligibilityAPI test_api = {
    .IsPageContextEligible = &MockIsPageContextEligible,
    .IsPageContextEligibleWithAccount = &MockIsPageContextEligibleWithAccount,
    .ShouldReextractPageContext = &MockShouldReextractPageContext,
    .GetMetaTagNamesAffectingEligibility = nullptr,
    .CheckPageEligibility = &MockCheckPageEligibility,
};

auto IsSingleFrame(const std::string& host, const std::string& path) {
  return testing::ElementsAre(
      testing::AllOf(testing::Field(&FrameUrl::host, host),
                     testing::Field(&FrameUrl::path, path)));
}

auto IsMultipleFrames(const std::string& host1,
                      const std::string& path1,
                      const std::string& host2,
                      const std::string& path2) {
  return testing::UnorderedElementsAre(
      testing::AllOf(testing::Field(&FrameUrl::host, host1),
                     testing::Field(&FrameUrl::path, path1)),
      testing::AllOf(testing::Field(&FrameUrl::host, host2),
                     testing::Field(&FrameUrl::path, path2)));
}

}  // namespace

class PageContextEligibilityObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    mock_api_ = std::make_unique<testing::StrictMock<MockEligibilityAPI>>();
    g_mock_api = mock_api_.get();
    test_eligibility_holder_ =
        std::make_unique<PageContextEligibility>(&test_api);
    PageContextEligibility::SetForTesting(test_eligibility_holder_.get());
  }

  void TearDown() override {
    PageContextEligibility::SetForTesting(nullptr);
    g_mock_api = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<PageContextEligibilityObserver> CreateObserver(
      std::string account = std::string(),
      base::RepeatingCallback<void(PageContextEligibilityStatus)> callback =
          base::DoNothing()) {
    auto observer = PageContextEligibilityObserver::Create(
        web_contents(), std::move(account), std::move(callback));
    task_environment()->RunUntilIdle();
    return observer;
  }

  MockEligibilityAPI* mock_api() { return mock_api_.get(); }

  void CallOnMetaTagsChanged(PageContextEligibilityObserver& observer,
                             blink::mojom::PageMetadataPtr metadata) {
    observer.OnMetaTagsChanged(std::move(metadata));
  }

 private:
  std::unique_ptr<MockEligibilityAPI> mock_api_;
  std::unique_ptr<PageContextEligibility> test_eligibility_holder_;
};

TEST_F(PageContextEligibilityObserverTest, AccountGetterExecution) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/path"));

  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsSingleFrame("www.example.com", "/path")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));
  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path",
                                               "test@example.com", testing::_))
      .WillRepeatedly(testing::Return(true));

  auto observer_ptr = CreateObserver("test@example.com");
  ASSERT_TRUE(observer_ptr);
  PageContextEligibilityObserver& observer = *observer_ptr;
  EXPECT_EQ(observer.IsPageContextEligible(),
            PageContextEligibilityStatus::kEligible);
}

TEST_F(PageContextEligibilityObserverTest, MetaTagParsingAndObserverUpdates) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/path"));

  static constexpr std::string_view kMetaTagNames[] = {"tagname"};
  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsSingleFrame("www.example.com", "/path")))
      .WillRepeatedly(testing::Return(PageEligibilityResult{
          .status = PageEligibility::kConditionalOnMetaTags,
          .meta_tag_names_affecting_eligibility = {.data = kMetaTagNames,
                                                   .size = 1}}));

  EXPECT_CALL(*mock_api(), IsPageContextEligibleWithAccount(
                               "www.example.com", "/path", "", testing::_))
      .WillRepeatedly(testing::Return(true));

  auto observer_ptr = CreateObserver();
  ASSERT_TRUE(observer_ptr);
  PageContextEligibilityObserver& observer = *observer_ptr;

  // Simulate meta tag update.
  auto page_metadata = blink::mojom::PageMetadata::New();
  auto frame_metadata = blink::mojom::FrameMetadata::New();
  frame_metadata->url = GURL("https://www.example.com/path");
  auto meta_tag = blink::mojom::MetaTag::New();
  meta_tag->name = "tagname";
  meta_tag->content = "tagvalue";
  frame_metadata->meta_tags.push_back(std::move(meta_tag));
  page_metadata->frame_metadata.push_back(std::move(frame_metadata));

  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path", "",
                                               testing::SizeIs(1)))
      .WillRepeatedly(testing::Return(false));

  CallOnMetaTagsChanged(observer, std::move(page_metadata));
  EXPECT_EQ(observer.IsPageContextEligible(),
            PageContextEligibilityStatus::kNotEligible);
}

TEST_F(PageContextEligibilityObserverTest, EligibilityChangeNotification) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/path"));

  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsSingleFrame("www.example.com", "/path")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));
  EXPECT_CALL(*mock_api(), IsPageContextEligibleWithAccount(
                               "www.example.com", "/path", "", testing::_))
      .WillRepeatedly(testing::Return(true));

  int callback_count = 0;
  PageContextEligibilityStatus last_eligibility =
      PageContextEligibilityStatus::kUnknown;

  auto observer_ptr = CreateObserver(
      std::string(), base::BindRepeating(
                         [](int* count, PageContextEligibilityStatus* last_val,
                            PageContextEligibilityStatus status) {
                           (*count)++;
                           *last_val = status;
                         },
                         &callback_count, &last_eligibility));
  ASSERT_TRUE(observer_ptr);
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(PageContextEligibilityStatus::kEligible, last_eligibility);
  PageContextEligibilityObserver& observer = *observer_ptr;

  // Change to false
  auto page_metadata = blink::mojom::PageMetadata::New();
  auto frame_metadata = blink::mojom::FrameMetadata::New();
  frame_metadata->url = GURL("https://www.example.com/path");
  page_metadata->frame_metadata.push_back(std::move(frame_metadata));

  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path", "",
                                               testing::SizeIs(1)))
      .WillOnce(testing::Return(false));

  CallOnMetaTagsChanged(observer, std::move(page_metadata));
  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(PageContextEligibilityStatus::kNotEligible, last_eligibility);

  // Redundant update (still false) should not trigger callback
  auto page_metadata2 = blink::mojom::PageMetadata::New();
  auto frame_metadata2 = blink::mojom::FrameMetadata::New();
  frame_metadata2->url = GURL("https://www.example.com/path");
  page_metadata2->frame_metadata.push_back(std::move(frame_metadata2));

  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path", "",
                                               testing::SizeIs(1)))
      .WillOnce(testing::Return(false));

  CallOnMetaTagsChanged(observer, std::move(page_metadata2));
  EXPECT_EQ(2, callback_count);  // Should not have incremented

  // Change back to true
  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path", "",
                                               testing::IsEmpty()))
      .WillOnce(testing::Return(true));
  CallOnMetaTagsChanged(observer, nullptr);
  EXPECT_EQ(3, callback_count);
  EXPECT_EQ(last_eligibility, PageContextEligibilityStatus::kEligible);
}

TEST_F(PageContextEligibilityObserverTest, NavigationResetsObserver) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/path"));

  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsSingleFrame("www.example.com", "/path")))
      .WillOnce(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));
  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path", "",
                                               testing::IsEmpty()))
      .WillOnce(testing::Return(true));

  auto observer_ptr = CreateObserver();
  ASSERT_TRUE(observer_ptr);

  // Navigate to a new page
  EXPECT_CALL(*mock_api(), CheckPageEligibility(
                               IsSingleFrame("www.another.com", "/newpath")))
      .WillOnce(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));
  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.another.com", "/newpath",
                                               "", testing::IsEmpty()))
      .WillOnce(testing::Return(true));

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.another.com/newpath"));

  // No explicit EXPECTs here, the strict mock will catch unexpected calls.
  // The fact that GetMetaTagNamesAffectingEligibility and
  // IsPageContextEligibleWithAccount are called with the new URL means the
  // observer properly reset and re-evaluated.
}

TEST_F(PageContextEligibilityObserverTest, SubframeNavigation) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/path"));

  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsSingleFrame("www.example.com", "/path")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));
  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path", "",
                                               testing::IsEmpty()))
      .WillRepeatedly(testing::Return(true));

  auto observer_ptr = CreateObserver();
  ASSERT_TRUE(observer_ptr);

  // Append a subframe
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("subframe");

  // Since a subframe was created (about:blank initially), we expect
  // CheckPageEligibility to be called with main frame and about:blank subframe.
  EXPECT_CALL(*mock_api(), CheckPageEligibility(IsMultipleFrames(
                               "www.example.com", "/path", "", "")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));

  // Now, navigate the subframe to a new URL
  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsMultipleFrames(
                  "www.example.com", "/path", "www.subframe.com", "/subpath")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.subframe.com/subpath"), subframe);
  navigation->Commit();
}

TEST_F(PageContextEligibilityObserverTest, SubframeDataUrlPrecursor) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/path"));

  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsSingleFrame("www.example.com", "/path")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));
  EXPECT_CALL(*mock_api(),
              IsPageContextEligibleWithAccount("www.example.com", "/path", "",
                                               testing::IsEmpty()))
      .WillRepeatedly(testing::Return(true));

  auto observer_ptr = CreateObserver();
  ASSERT_TRUE(observer_ptr);

  // Append a subframe
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("subframe");

  // Since a subframe was created (about:blank initially), we expect
  // CheckPageEligibility
  EXPECT_CALL(*mock_api(), CheckPageEligibility(IsMultipleFrames(
                               "www.example.com", "/path", "", "")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));

  // Now, navigate the subframe to a data URL.
  // The precursor origin should be parsed, yielding the precursor origin URL
  // "https://www.example.com" which has host "www.example.com" and path "/".
  EXPECT_CALL(*mock_api(),
              CheckPageEligibility(IsMultipleFrames("www.example.com", "/path",
                                                    "www.example.com", "/")))
      .WillRepeatedly(testing::Return(
          PageEligibilityResult{.status = PageEligibility::kEligible,
                                .meta_tag_names_affecting_eligibility = {
                                    .data = nullptr, .size = 0}}));

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("data:text/html,<html></html>"), subframe);
  navigation->Commit();
}

}  // namespace optimization_guide
