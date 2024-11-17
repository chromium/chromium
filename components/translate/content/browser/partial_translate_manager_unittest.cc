// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/partial_translate_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/contextual_search/core/browser/contextual_search_delegate.h"
#include "components/contextual_search/core/browser/resolved_search_term.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeContextualSearchDelegate : public ContextualSearchDelegate {
 public:
  void GatherAndSaveSurroundingText(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents,
      SurroundingTextCallback callback) override {
    // Unused.
  }

  void StartSearchTermResolutionRequest(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents,
      ContextualSearchDelegate::SearchTermResolutionCallback callback)
      override {
    context_ = contextual_search_context;
    search_term_callback_ = std::move(callback);
  }

  const ContextualSearchContext& GetContext() { return *context_; }

  void RunSearchTermCallback(const ResolvedSearchTerm& resolved_search_term) {
    search_term_callback_.Run(resolved_search_term);
  }

 private:
  base::WeakPtr<ContextualSearchContext> context_;
  ContextualSearchDelegate::SearchTermResolutionCallback search_term_callback_;
};

class PartialTranslateManagerTest : public testing::Test {
 public:
  PartialTranslateManagerTest() = default;

  void SetUp() override {
    auto delegate = std::make_unique<FakeContextualSearchDelegate>();
    delegate_ = delegate.get();
    manager_ = std::make_unique<PartialTranslateManager>(std::move(delegate));
  }

  void TearDown() override {}

 protected:
  std::unique_ptr<PartialTranslateManager> manager_;
  // Owned by manager_.
  raw_ptr<FakeContextualSearchDelegate> delegate_;
};

TEST_F(PartialTranslateManagerTest, CreateContext) {
  PartialTranslateRequest request;
  request.selection_text = u"Selected text";
  request.selection_encoding = "UTF16";
  request.source_language = "en-US";
  request.target_language = "ja-JP";
  request.apply_lang_hint = true;

  manager_->StartPartialTranslate(nullptr, request, base::DoNothing());

  const ContextualSearchContext& context = delegate_->GetContext();
  ASSERT_EQ(context.GetRequestType(),
            ContextualSearchContext::RequestType::PARTIAL_TRANSLATE);
  ASSERT_EQ(context.GetSurroundingText(), u"Selected text");
  ASSERT_EQ(context.GetTranslationLanguages().detected_language, "en-US");
  ASSERT_EQ(context.GetTranslationLanguages().target_language, "ja-JP");
  ASSERT_TRUE(context.GetApplyLangHint());
}

TEST_F(PartialTranslateManagerTest, CreateResponse) {
  PartialTranslateRequest request;
  request.selection_text = u"Selected text";
  request.selection_encoding = "UTF16";
  request.source_language = "en-US";
  request.target_language = "ja-JP";

  PartialTranslateResponse response;
  PartialTranslateManager::PartialTranslateCallback callback = base::BindOnce(
      [](PartialTranslateResponse* out_response,
         const PartialTranslateResponse& in_response) {
        *out_response = in_response;
      },
      &response);
  manager_->StartPartialTranslate(nullptr, request, std::move(callback));

  ResolvedSearchTerm resolved_search_term(
      /*is_invalid=*/false,
      /*response_code=*/200,
      /*search_term=*/"",
      /*display_text=*/"",
      /*alternate_term=*/"",
      /*mid=*/"",
      /*prevent_preload=*/false,
      /*selection_start_adjust=*/0,
      /*selection_end_adjust=*/0,
      /*context_language=*/"en-CA",
      /*thumbnail_url=*/"",
      /*caption=*/"Translated",
      /*quick_action_uri=*/"",
      /*quick_action_category=*/QUICK_ACTION_CATEGORY_NONE,
      /*search_url_full=*/"",
      /*search_url_preload=*/"",
      /*coca_card_tag=*/200,
      /*related_searches_json=*/"");
  delegate_->RunSearchTermCallback(resolved_search_term);

  ASSERT_EQ(response.translated_text, u"Translated");
  ASSERT_EQ(response.source_language, "en-CA");
  ASSERT_EQ(response.target_language, "ja-JP");
}

TEST_F(PartialTranslateManagerTest, SubsumeRequest) {
  bool first_ran = false;
  PartialTranslateManager::PartialTranslateCallback first_callback =
      base::BindOnce(
          [](bool* callback_ran, const PartialTranslateResponse&) {
            *callback_ran = true;
          },
          &first_ran);
  bool second_ran = false;
  PartialTranslateManager::PartialTranslateCallback second_callback =
      base::BindOnce(
          [](bool* callback_ran, const PartialTranslateResponse&) {
            *callback_ran = true;
          },
          &second_ran);
  manager_->StartPartialTranslate(nullptr, PartialTranslateRequest(),
                                  std::move(first_callback));
  // Start another request before the first one returns.
  manager_->StartPartialTranslate(nullptr, PartialTranslateRequest(),
                                  std::move(second_callback));
  ASSERT_FALSE(first_ran);
  ASSERT_FALSE(second_ran);
  delegate_->RunSearchTermCallback(ResolvedSearchTerm(200));
  ASSERT_FALSE(first_ran);
  ASSERT_TRUE(second_ran);
}

}  // namespace
