// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/home_button.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

// HomePageUndoBubble --------------------------------------------------------

namespace {

class HomePageUndoBubble : public views::BubbleDialogDelegateView,
                           public views::StyledLabelListener {
 public:
  HomePageUndoBubble(const HomePageUndoBubble&) = delete;
  HomePageUndoBubble& operator=(const HomePageUndoBubble&) = delete;

  static void ShowBubble(Browser* browser,
                         bool undo_value_is_ntp,
                         const GURL& undo_url,
                         views::View* anchor_view);
  static void HideBubble();

 private:
  HomePageUndoBubble(Browser* browser, bool undo_value_is_ntp,
                     const GURL& undo_url, views::View* anchor_view);
  ~HomePageUndoBubble() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  static HomePageUndoBubble* home_page_undo_bubble_;

  Browser* browser_;
  bool undo_value_is_ntp_;
  GURL undo_url_;
};

// static
HomePageUndoBubble* HomePageUndoBubble::home_page_undo_bubble_ = nullptr;

void HomePageUndoBubble::ShowBubble(Browser* browser,
                                    bool undo_value_is_ntp,
                                    const GURL& undo_url,
                                    views::View* anchor_view) {
  HideBubble();
  home_page_undo_bubble_ = new HomePageUndoBubble(browser,
                                                  undo_value_is_ntp,
                                                  undo_url,
                                                  anchor_view);
  views::BubbleDialogDelegateView::CreateBubble(home_page_undo_bubble_)->Show();
}

void HomePageUndoBubble::HideBubble() {
  if (home_page_undo_bubble_)
    home_page_undo_bubble_->GetWidget()->Close();
}

HomePageUndoBubble::HomePageUndoBubble(
    Browser* browser,
    bool undo_value_is_ntp,
    const GURL& undo_url,
    views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      browser_(browser),
      undo_value_is_ntp_(undo_value_is_ntp),
      undo_url_(undo_url) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
  set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::HOME_PAGE_UNDO);
}

HomePageUndoBubble::~HomePageUndoBubble() = default;

void HomePageUndoBubble::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  base::string16 undo_string =
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_BUBBLE_UNDO);
  std::vector<base::string16> message = {
      l10n_util::GetStringUTF16(IDS_TOOLBAR_INFORM_SET_HOME_PAGE), undo_string};
  views::StyledLabel* label = new views::StyledLabel(
      base::JoinString(message, base::StringPiece16(base::ASCIIToUTF16(" "))),
      this);

  gfx::Range undo_range(label->GetText().length() - undo_string.length(),
                        label->GetText().length());
  label->AddStyleRange(undo_range,
                       views::StyledLabel::RangeStyleInfo::CreateForLink());

  // Ensure StyledLabel has a cached size to return in GetPreferredSize().
  label->SizeToFit(0);
  AddChildView(label);
}

void HomePageUndoBubble::StyledLabelLinkClicked(views::StyledLabel* label,
                                                const gfx::Range& range,
                                                int event_flags) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_->profile());
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, undo_value_is_ntp_);
  prefs->SetString(prefs::kHomePage, undo_url_.spec());

  HideBubble();
}

void HomePageUndoBubble::WindowClosing() {
  // We have to reset |home_page_undo_bubble_| here, not in our destructor,
  // because we'll be hidden first, then destroyed asynchronously.  If we wait
  // to reset this, and the user triggers a call to ShowBubble() while the
  // window is hidden but not destroyed, GetWidget()->Close() would be
  // called twice.
  DCHECK_EQ(this, home_page_undo_bubble_);
  home_page_undo_bubble_ = nullptr;
}

}  // namespace


// HomeButton -----------------------------------------------------------

HomeButton::HomeButton(views::ButtonListener* listener, Browser* browser)
    : ToolbarButton(listener), browser_(browser) {}

HomeButton::~HomeButton() {
}

const char* HomeButton::GetClassName() const {
  return "HomeButton";
}

bool HomeButton::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::URL;
  return true;
}

bool HomeButton::CanDrop(const OSExchangeData& data) {
  return data.HasURL(ui::OSExchangeData::CONVERT_FILENAMES);
}

int HomeButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  return event.source_operations();
}

int HomeButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  GURL new_homepage_url;
  base::string16 title;
  if (event.data().GetURLAndTitle(
          ui::OSExchangeData::CONVERT_FILENAMES, &new_homepage_url, &title) &&
      new_homepage_url.is_valid()) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);
    GURL old_homepage(prefs->GetString(prefs::kHomePage));

    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
    prefs->SetString(prefs::kHomePage, new_homepage_url.spec());

    HomePageUndoBubble::ShowBubble(browser_, old_is_ntp, old_homepage, this);
  }
  return ui::DragDropTypes::DRAG_NONE;
}
