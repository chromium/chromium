// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/omnibox/test_location_bar.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace {

class TestOmniboxPopupWebUIBaseContent : public OmniboxPopupWebUIBaseContent {
 public:
  TestOmniboxPopupWebUIBaseContent(LocationBar* location_bar,
                                   OmniboxController* controller)
      : OmniboxPopupWebUIBaseContent(/*content_delegate=*/nullptr,
                                     location_bar,
                                     controller,
                                     /*top_rounded_corners=*/true) {}
  TestOmniboxPopupWebUIBaseContent(const TestOmniboxPopupWebUIBaseContent&) =
      delete;
  TestOmniboxPopupWebUIBaseContent& operator=(
      const TestOmniboxPopupWebUIBaseContent&) = delete;
  ~TestOmniboxPopupWebUIBaseContent() override = default;

  // OmniboxPopupWebUIBaseContent:
  void OnContextMenuClosed() override {}
  void Clear() override {}
  std::string_view GetMetricPrefix() const override { return "TestPrefix"; }
};

class OmniboxPopupWebUIBaseContentTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = TestingProfile::Builder().Build();
    location_bar_.set_profile(profile_.get());
    controller_ = std::make_unique<OmniboxController>(
        std::make_unique<TestOmniboxClient>());
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
  }

  void TearDown() override {
    content_.reset();
    web_contents_factory_.DestroyWebContents(web_contents_.ExtractAsDangling());
    controller_.reset();
    location_bar_.set_profile(nullptr);
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  TestLocationBar location_bar_;
  std::unique_ptr<OmniboxController> controller_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<TestOmniboxPopupWebUIBaseContent> content_;
};

TEST_F(OmniboxPopupWebUIBaseContentTest, NullSizerSkipsBoundsSyncAndResize) {
  content_ = std::make_unique<TestOmniboxPopupWebUIBaseContent>(
      &location_bar_, controller_.get());
  content_->SetWebContents(web_contents_);

  content_->OnLocationBarBoundsChanged();
  content_->ResizeDueToAutoResize(web_contents_, gfx::Size(400, 300));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(gfx::Size(400, 300), content_->GetPreferredSize({}));
}

}  // namespace
