// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar_with_normal_label.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar_with_styled_label.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/event.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"
namespace {

class FakeInfoBarManager : public infobars::InfoBarManager {
 public:
  FakeInfoBarManager() = default;
  ~FakeInfoBarManager() override = default;

  int GetActiveEntryID() override { return 0; }
  void OpenURL(const GURL& url,
               WindowOpenDisposition disposition,
               const std::string& text_fragment) override {}
};

class FakeConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  FakeConfirmInfoBarDelegate() = default;
  ~FakeConfirmInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return infobars::InfoBarDelegate::TEST_INFOBAR;
  }

  std::u16string GetMessageText() const override { return u"Standard Message"; }
  std::u16string GetMessageTextTemplate() const override { return u""; }
  gfx::ElideBehavior GetMessageElideBehavior() const override {
    return gfx::ELIDE_TAIL;
  }
};

class InlineSubstitutionTestDelegate : public FakeConfirmInfoBarDelegate {
 public:
  // `clicked_index_out` is used to safely extract the clicked link index.
  // Because InfoBars destroy their delegates when closed, storing the
  // clicked state internally would result in a use-after-free when tests
  // attempt to verify it.
  InlineSubstitutionTestDelegate(
      std::u16string text_template,
      std::vector<MessageSubstitution> substitutions,
      std::optional<size_t>* clicked_index_out = nullptr)
      : template_(text_template),
        substitutions_(std::move(substitutions)),
        clicked_index_out_(clicked_index_out) {}

  std::u16string GetMessageTextTemplate() const override { return template_; }
  std::vector<MessageSubstitution> GetMessageSubstitutions() const override {
    return substitutions_;
  }

  bool InlineSubstitutionLinkClicked(
      size_t index,
      WindowOpenDisposition disposition) override {
    if (clicked_index_out_) {
      *clicked_index_out_ = index;
    }
    return true;
  }

 private:
  std::u16string template_;
  std::vector<MessageSubstitution> substitutions_;
  raw_ptr<std::optional<size_t>> clicked_index_out_;
};

}  // namespace

class ConfirmInfoBarTest : public views::ViewsTestBase {
 protected:
  ConfirmInfoBarTest() = default;
  ~ConfirmInfoBarTest() override = default;

  // Instantiate the global layout singleton required for InfoBar layout
  // construction.
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    layout_provider_ = std::make_unique<ChromeLayoutProvider>();
  }

  base::test::ScopedFeatureList feature_list_;

  views::Label* CreateNormalLabelInfoBarAndGetLabel() {
    feature_list_.InitAndDisableFeature(features::kInfoBarInlineLinks);
    auto delegate = std::make_unique<FakeConfirmInfoBarDelegate>();
    infobar_ = ConfirmInfoBar::Create(std::move(delegate));
    auto* standard_infobar =
        static_cast<ConfirmInfoBarWithNormalLabel*>(infobar_.get());
    return standard_infobar->label_for_testing();
  }

  views::StyledLabel* CreateStyledLabelInfoBarAndGetLabel() {
    feature_list_.InitAndEnableFeature(features::kInfoBarInlineLinks);
    std::vector<MessageSubstitution> substitutions;
    substitutions.emplace_back(u"link", true, std::nullopt);
    substitutions.emplace_back(u"text", false, std::nullopt);
    auto delegate = std::make_unique<InlineSubstitutionTestDelegate>(
        u"Message with $1 and $2", std::move(substitutions));
    infobar_ = ConfirmInfoBar::Create(std::move(delegate));
    auto* inline_infobar =
        static_cast<ConfirmInfoBarWithStyledLabel*>(infobar_.get());
    return inline_infobar->styled_label_for_testing();
  }

 private:
  std::unique_ptr<ChromeLayoutProvider> layout_provider_;
  std::unique_ptr<ConfirmInfoBar> infobar_;
};

TEST_F(ConfirmInfoBarTest, StandardMessageUsesLabel) {
  views::Label* message_label = CreateNormalLabelInfoBarAndGetLabel();
  ASSERT_NE(nullptr, message_label);
  EXPECT_EQ(u"Standard Message", message_label->GetText());
}

TEST_F(ConfirmInfoBarTest, StandardMessageUsesEliding) {
  views::Label* message_label = CreateNormalLabelInfoBarAndGetLabel();
  ASSERT_NE(nullptr, message_label);
  EXPECT_EQ(gfx::ELIDE_TAIL, message_label->GetElideBehavior());
}

using ConfirmInfoBarWithInlineLinksTest = ConfirmInfoBarTest;

// Verifies that a message with substitutions and links correctly creates a
// StyledLabel instead of a standard Label.
TEST_F(ConfirmInfoBarWithInlineLinksTest, TemplateMessageUsesStyledLabel) {
  views::StyledLabel* message_label = CreateStyledLabelInfoBarAndGetLabel();
  ASSERT_NE(nullptr, message_label);
  EXPECT_EQ(u"Message with link and text", message_label->GetText());
}

// Verifies that link styling is applied to the StyledLabel by checking
// for the existence of a link child view.
TEST_F(ConfirmInfoBarWithInlineLinksTest, TemplateMessageAppliesLinkStyles) {
  views::StyledLabel* message_label = CreateStyledLabelInfoBarAndGetLabel();
  ASSERT_NE(nullptr, message_label);

  // Force a layout pass to instantiate child views.
  message_label->SizeToFit(0);

  // Check that the StyledLabel contains a link child.
  EXPECT_TRUE(message_label->GetFirstLinkForTesting());
}

// Verifies that clicking on the StyledLabel's link triggers the expected
// callback in the delegate.
TEST_F(ConfirmInfoBarWithInlineLinksTest,
       TemplateMessageLinkClickTriggersCallback) {
  // Set up an InlineInfoBar with a mock manager and out-parameter.
  FakeInfoBarManager manager;
  std::optional<size_t> clicked_index;
  std::vector<MessageSubstitution> substitutions;
  substitutions.emplace_back(u"link", true, std::nullopt);
  auto delegate = std::make_unique<InlineSubstitutionTestDelegate>(
      u"Message with $1", std::move(substitutions), &clicked_index);

  feature_list_.InitAndEnableFeature(features::kInfoBarInlineLinks);
  auto* infobar = static_cast<ConfirmInfoBarWithStyledLabel*>(
      manager.AddInfoBar(ConfirmInfoBar::Create(std::move(delegate))));

  views::StyledLabel* message_label = infobar->styled_label_for_testing();
  ASSERT_NE(nullptr, message_label);

  // Force a layout pass to instantiate child views.
  message_label->SizeToFit(0);

  // Simulate a click on the first link.
  message_label->ClickFirstLinkForTesting();

  EXPECT_EQ(0u, clicked_index);
}
