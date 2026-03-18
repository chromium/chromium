// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class WebUIReadOnlyOmniboxTest : public testing::Test {
 protected:
  TestLocationBarModel* location_bar_model() {
    return omnibox_client_->location_bar_model();
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  content::BrowserTaskEnvironment browser_threads_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
  std::unique_ptr<WebUIReadOnlyOmnibox> omnibox_view_;

  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> wc1_;
  raw_ptr<content::WebContents> wc2_;
};

void WebUIReadOnlyOmniboxTest::SetUp() {
  profile_ = TestingProfile::Builder().Build();

  auto omnibox_client = std::make_unique<TestOmniboxClient>();
  omnibox_client_ = omnibox_client.get();
  omnibox_controller_ =
      std::make_unique<OmniboxController>(std::move(omnibox_client));

  omnibox_view_ = std::make_unique<WebUIReadOnlyOmnibox>(
      omnibox_controller_.get(), /*location_bar=*/nullptr);

  wc1_ = web_contents_factory_.CreateWebContents(profile_.get());
  wc2_ = web_contents_factory_.CreateWebContents(profile_.get());
}

void WebUIReadOnlyOmniboxTest::TearDown() {
  web_contents_factory_.DestroyWebContents(wc1_.ExtractAsDangling());
  web_contents_factory_.DestroyWebContents(wc2_.ExtractAsDangling());
}

TEST_F(WebUIReadOnlyOmniboxTest, StateManagement) {
  std::u16string partial = u"https://uk.wikipe";
  omnibox_view_->SetUserText(partial);
  omnibox_view_->SetCaretPos(partial.size());
  EXPECT_FALSE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(partial, omnibox_view_->GetText());
  omnibox_view_->SaveStateToTab(wc1_);

  std::u16string complete = u"https://chromium.org";
  omnibox_view_->SetUserText(complete);
  omnibox_view_->SelectAll(/*reversed=*/false);
  EXPECT_TRUE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(complete, omnibox_view_->GetText());
  omnibox_view_->SaveStateToTab(wc2_);

  // Emulate switching back to wc1.
  omnibox_view_->OnTabChanged(wc1_);
  EXPECT_FALSE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(partial, omnibox_view_->GetText());
  EXPECT_EQ(omnibox_view_->GetSelectionBounds(), gfx::Range(partial.size()));

  // Switch back to wc2.
  omnibox_view_->OnTabChanged(wc2_);
  EXPECT_TRUE(omnibox_view_->IsSelectAll());
  EXPECT_EQ(complete, omnibox_view_->GetText());
  EXPECT_EQ(omnibox_view_->GetSelectionBounds(),
            gfx::Range(0, complete.size()));

  // If no saved state, pulls from the location bar model.
  std::u16string navigated_to = u"https://developer.mozilla.org/";
  location_bar_model()->set_url_for_display(navigated_to);
  omnibox_view_->ResetTabState(wc2_);
  omnibox_view_->OnTabChanged(wc2_);
  EXPECT_EQ(navigated_to, omnibox_view_->GetText());
  EXPECT_EQ(gfx::Range(), omnibox_view_->GetSelectionBounds());

  // Update() can pull further changes.
  std::u16string navigated_to2 = u"https://developer.mozilla.org/en-US";
  location_bar_model()->set_url_for_display(navigated_to2);
  omnibox_view_->Update();
  EXPECT_EQ(navigated_to2, omnibox_view_->GetText());
}

TEST_F(WebUIReadOnlyOmniboxTest, GetOmniboxTextLength) {
  std::u16string partial = u"https://uk.wikipe";
  omnibox_view_->SetUserText(partial);
  EXPECT_EQ(partial.size(),
            static_cast<size_t>(omnibox_view_->GetOmniboxTextLength()));
}
