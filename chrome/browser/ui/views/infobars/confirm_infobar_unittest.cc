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
#include "chrome/browser/ui/views/infobars/confirm_infobar_with_custom_view.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar_with_normal_label.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar_with_styled_label.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
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

class ExtraButtonTestDelegate : public FakeConfirmInfoBarDelegate {
 public:
  explicit ExtraButtonTestDelegate(bool* extra_button_pressed_out = nullptr,
                                   int buttons = BUTTON_OK | BUTTON_CANCEL |
                                                 BUTTON_EXTRA,
                                   bool close_on_extra_button = true,
                                   bool use_text_color_for_extra_icon = true,
                                   ui::ImageModel button_image = {})
      : extra_button_pressed_out_(extra_button_pressed_out),
        buttons_(buttons),
        close_on_extra_button_(close_on_extra_button),
        use_text_color_for_extra_icon_(use_text_color_for_extra_icon),
        button_image_(std::move(button_image)) {}

  int GetButtons() const override { return buttons_; }

  std::u16string GetButtonLabel(InfoBarButton button) const override {
    switch (button) {
      case BUTTON_OK:
        return u"OK";
      case BUTTON_CANCEL:
        return u"Cancel";
      case BUTTON_EXTRA:
        return u"Extra";
      case BUTTON_NONE:
        break;
    }
    return std::u16string();
  }

  ui::ImageModel GetButtonImage(InfoBarButton button) const override {
    return button_image_;
  }

  bool ShouldUseTextColorForButtonIcon(InfoBarButton button) const override {
    if (button == BUTTON_EXTRA) {
      return use_text_color_for_extra_icon_;
    }
    return ConfirmInfoBarDelegate::ShouldUseTextColorForButtonIcon(button);
  }

  bool ExtraButtonPressed() override {
    if (extra_button_pressed_out_) {
      *extra_button_pressed_out_ = true;
    }
    return close_on_extra_button_;
  }

 private:
  raw_ptr<bool> extra_button_pressed_out_;
  int buttons_;
  bool close_on_extra_button_;
  bool use_text_color_for_extra_icon_;
  ui::ImageModel button_image_;
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
  const std::vector<MessageSubstitution>& GetMessageSubstitutions()
      const override {
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

TEST_F(ConfirmInfoBarTest, CustomMessageViewAndExtraButton) {
  auto delegate = std::make_unique<ExtraButtonTestDelegate>();
  auto custom_view = std::make_unique<views::View>();
  views::View* raw_custom_view = custom_view.get();
  std::unique_ptr<ConfirmInfoBar> infobar =
      ConfirmInfoBar::Create(std::move(delegate), std::move(custom_view));

  ASSERT_TRUE(views::IsViewClass<ConfirmInfoBarWithCustomView>(infobar.get()));
  EXPECT_EQ(raw_custom_view,
            static_cast<ConfirmInfoBarWithCustomView*>(infobar.get())
                ->custom_view_for_testing());
  EXPECT_EQ(raw_custom_view, infobar->message_view_for_testing());
  EXPECT_EQ(nullptr, infobar->label_for_testing());
  ASSERT_NE(nullptr, infobar->ok_button_for_testing());
  ASSERT_NE(nullptr, infobar->cancel_button_for_testing());
  ASSERT_NE(nullptr, infobar->extra_button_for_testing());
  EXPECT_EQ(u"Extra", infobar->extra_button_for_testing()->GetText());

  std::unique_ptr<ConfirmInfoBar> fallback_infobar = ConfirmInfoBar::Create(
      std::make_unique<ExtraButtonTestDelegate>(), nullptr);
  EXPECT_TRUE(views::IsViewClass<ConfirmInfoBarWithNormalLabel>(
      fallback_infobar.get()));

  FakeConfirmInfoBarDelegate default_delegate;
  EXPECT_TRUE(
      default_delegate.GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_EXTRA)
          .empty());
}

TEST_F(ConfirmInfoBarTest, ExtraButtonClickTriggersCallback) {
  FakeInfoBarManager manager;

  bool extra_button_pressed = false;
  auto keep_open_delegate = std::make_unique<ExtraButtonTestDelegate>(
      &extra_button_pressed,
      ConfirmInfoBarDelegate::BUTTON_OK |
          ConfirmInfoBarDelegate::BUTTON_CANCEL |
          ConfirmInfoBarDelegate::BUTTON_EXTRA,
      /*close_on_extra_button=*/false);
  auto* infobar = static_cast<ConfirmInfoBar*>(manager.AddInfoBar(
      ConfirmInfoBar::Create(std::move(keep_open_delegate))));

  ASSERT_NE(nullptr, infobar->extra_button_for_testing());
  views::test::ButtonTestApi(infobar->extra_button_for_testing())
      .NotifyDefaultMouseClick();

  EXPECT_TRUE(extra_button_pressed);
  EXPECT_EQ(1u, manager.infobars().size());
  manager.RemoveInfoBar(infobar);
  ASSERT_TRUE(manager.infobars().empty());

  extra_button_pressed = false;
  auto close_delegate = std::make_unique<ExtraButtonTestDelegate>(
      &extra_button_pressed,
      ConfirmInfoBarDelegate::BUTTON_OK |
          ConfirmInfoBarDelegate::BUTTON_CANCEL |
          ConfirmInfoBarDelegate::BUTTON_EXTRA,
      /*close_on_extra_button=*/true);
  infobar = static_cast<ConfirmInfoBar*>(
      manager.AddInfoBar(ConfirmInfoBar::Create(std::move(close_delegate))));

  ASSERT_NE(nullptr, infobar->extra_button_for_testing());
  views::test::ButtonTestApi(infobar->extra_button_for_testing())
      .NotifyDefaultMouseClick();

  EXPECT_TRUE(extra_button_pressed);
  EXPECT_TRUE(manager.infobars().empty());
}

TEST_F(ConfirmInfoBarTest, ExtraButtonMargins) {
  const int expected_button_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  std::unique_ptr<ConfirmInfoBar> multi_button_infobar =
      ConfirmInfoBar::Create(std::make_unique<ExtraButtonTestDelegate>());
  ASSERT_NE(nullptr, multi_button_infobar->extra_button_for_testing());
  const gfx::Insets* multi_margins =
      multi_button_infobar->extra_button_for_testing()->GetProperty(
          views::kMarginsKey);
  ASSERT_NE(nullptr, multi_margins);
  EXPECT_EQ(expected_button_spacing, multi_margins->left());

  for (int buttons : {ConfirmInfoBarDelegate::BUTTON_OK |
                          ConfirmInfoBarDelegate::BUTTON_EXTRA,
                      ConfirmInfoBarDelegate::BUTTON_CANCEL |
                          ConfirmInfoBarDelegate::BUTTON_EXTRA}) {
    std::unique_ptr<ConfirmInfoBar> two_button_infobar =
        ConfirmInfoBar::Create(std::make_unique<ExtraButtonTestDelegate>(
            /*extra_button_pressed_out=*/nullptr, buttons));
    ASSERT_NE(nullptr, two_button_infobar->extra_button_for_testing());
    const gfx::Insets* two_button_margins =
        two_button_infobar->extra_button_for_testing()->GetProperty(
            views::kMarginsKey);
    ASSERT_NE(nullptr, two_button_margins);
    EXPECT_EQ(expected_button_spacing, two_button_margins->left());
  }

  std::unique_ptr<ConfirmInfoBar> extra_only_infobar =
      ConfirmInfoBar::Create(std::make_unique<ExtraButtonTestDelegate>(
          /*extra_button_pressed_out=*/nullptr,
          ConfirmInfoBarDelegate::BUTTON_EXTRA));
  EXPECT_EQ(nullptr, extra_only_infobar->ok_button_for_testing());
  EXPECT_EQ(nullptr, extra_only_infobar->cancel_button_for_testing());
  ASSERT_NE(nullptr, extra_only_infobar->extra_button_for_testing());
  const gfx::Insets* extra_only_margins =
      extra_only_infobar->extra_button_for_testing()->GetProperty(
          views::kMarginsKey);
  ASSERT_NE(nullptr, extra_only_margins);
  EXPECT_EQ(0, extra_only_margins->left());
}

TEST_F(ConfirmInfoBarTest, ShouldUseTextColorForButtonIcon) {
  const ui::ImageModel icon_model = ui::ImageModel::FromVectorIcon(
      views::kInfoIcon, ui::kColorSysPrimary, /*icon_size=*/16);
  auto delegate = std::make_unique<ExtraButtonTestDelegate>(
      /*extra_button_pressed_out=*/nullptr,
      ConfirmInfoBarDelegate::BUTTON_OK | ConfirmInfoBarDelegate::BUTTON_EXTRA,
      /*close_on_extra_button=*/true,
      /*use_text_color_for_extra_icon=*/false, icon_model);

  std::unique_ptr<ConfirmInfoBar> infobar =
      ConfirmInfoBar::Create(std::move(delegate));
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetContentsView(infobar.get());

  ASSERT_NE(nullptr, infobar->ok_button_for_testing());
  ASSERT_NE(nullptr, infobar->extra_button_for_testing());

  EXPECT_EQ(icon_model, infobar->extra_button_for_testing()->GetImageModel(
                            views::Button::STATE_NORMAL));
  EXPECT_EQ(ui::ImageModel::FromVectorIcon(
                views::kInfoIcon,
                infobar->ok_button_for_testing()->GetCurrentTextColor(),
                /*icon_size=*/16),
            infobar->ok_button_for_testing()->GetImageModel(
                views::Button::STATE_NORMAL));
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
