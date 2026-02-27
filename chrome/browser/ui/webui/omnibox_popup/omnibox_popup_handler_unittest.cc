// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxPopupHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  OmniboxPopupHandlerTest() = default;
  ~OmniboxPopupHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());
    omnibox_popup_ui_ = std::make_unique<OmniboxPopupUI>(&web_ui_);
    handler_ = std::make_unique<OmniboxPopupHandler>(
        mojo::PendingReceiver<omnibox_popup::mojom::PageHandler>(),
        page_.BindAndGetRemote());
  }

  void TearDown() override {
    handler_.reset();
    omnibox_popup_ui_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  content::TestWebUI web_ui_;
  std::unique_ptr<OmniboxPopupUI> omnibox_popup_ui_;
  testing::NiceMock<MockOmniboxPopupPage> page_;
  std::unique_ptr<OmniboxPopupHandler> handler_;
};

TEST_F(OmniboxPopupHandlerTest, OnShow) {
  EXPECT_CALL(page_, OnShow());
  handler_->OnShow();
  page_.FlushForTesting();
}
