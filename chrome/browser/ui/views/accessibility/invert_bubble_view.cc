// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kHighContrastExtensionUrl[] =
    "https://chrome.google.com/webstore/detail/"
    "djcfdncoelnlbldjfhinnjlhdjlikmph";
constexpr char kDarkThemeSearchUrl[] =
    "https://chrome.google.com/webstore/category/collection/dark_themes";
constexpr char kLearnMoreUrl[] =
    "https://groups.google.com/a/googleproductforums.com/d/topic/chrome/"
    "Xrco2HsXS-8/discussion";

// Tag value used to uniquely identify the "learn more" (?) button.
constexpr int kLearnMoreButton = 100;

std::unique_ptr<views::View> CreateExtraView(views::ButtonListener* listener) {
  auto learn_more = views::CreateVectorImageButtonWithNativeTheme(
      listener, vector_icons::kHelpOutlineIcon);
  learn_more->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more->set_tag(kLearnMoreButton);
  return learn_more;
}

class InvertBubbleView : public views::BubbleDialogDelegateView,
                         public views::ButtonListener {
 public:
  InvertBubbleView(Browser* browser, views::View* anchor_view);
  ~InvertBubbleView() override;

 private:
  // Overridden from views::BubbleDialogDelegateView:
  void Init() override;

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void OpenLink(const std::string& url, const ui::Event& event);

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(InvertBubbleView);
};

InvertBubbleView::InvertBubbleView(Browser* browser, views::View* anchor_view)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      browser_(browser) {
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_DONE));
  SetExtraView(::CreateExtraView(this));
  set_margins(gfx::Insets());
  chrome::RecordDialogCreation(chrome::DialogIdentifier::INVERT);
}

InvertBubbleView::~InvertBubbleView() {
}

void InvertBubbleView::Init() {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(views::INSETS_DIALOG)));

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_HIGH_CONTRAST_HEADER),
      views::style::CONTEXT_DIALOG_BODY_TEXT));

  auto* high_contrast = AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_HIGH_CONTRAST_EXT),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  high_contrast->set_callback(base::BindRepeating(&InvertBubbleView::OpenLink,
                                                  base::Unretained(this),
                                                  kHighContrastExtensionUrl));

  auto* dark_theme = AddChildView(
      std::make_unique<views::Link>(l10n_util::GetStringUTF16(IDS_DARK_THEME),
                                    views::style::CONTEXT_DIALOG_BODY_TEXT));
  dark_theme->set_callback(base::BindRepeating(&InvertBubbleView::OpenLink,
                                               base::Unretained(this),
                                               kDarkThemeSearchUrl));

  // Switching to high-contrast mode has a nasty habit of causing Chrome
  // top-level windows to lose focus, so closing the bubble on deactivate
  // makes it disappear before the user has even seen it. This forces the
  // user to close it explicitly, which should be okay because it affects
  // a small minority of users, and only once.
  set_close_on_deactivate(false);
}

base::string16 InvertBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_HIGH_CONTRAST_TITLE);
}

bool InvertBubbleView::ShouldShowCloseButton() const {
  return true;
}

void InvertBubbleView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender->tag() == kLearnMoreButton)
    OpenLink(kLearnMoreUrl, event);
}

void InvertBubbleView::OpenLink(const std::string& url,
                                const ui::Event& event) {
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  content::OpenURLParams params(GURL(url), content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_LINK, false);
  browser_->OpenURL(params);
}

}  // namespace

void MaybeShowInvertBubbleView(BrowserView* browser_view) {
  Browser* browser = browser_view->browser();
  PrefService* pref_service = browser->profile()->GetPrefs();
  views::View* anchor =
      browser_view->toolbar_button_provider()->GetAppMenuButton();
  if (anchor && anchor->GetWidget() &&
      anchor->GetNativeTheme()->GetPlatformHighContrastColorScheme() ==
          ui::NativeTheme::PlatformHighContrastColorScheme::kDark &&
      !pref_service->GetBoolean(prefs::kInvertNotificationShown)) {
    pref_service->SetBoolean(prefs::kInvertNotificationShown, true);
    views::BubbleDialogDelegateView::CreateBubble(
        std::make_unique<InvertBubbleView>(browser, anchor))
        ->Show();
  }
}
