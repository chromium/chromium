// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/home_button.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

// HomePageUndoBubble --------------------------------------------------------

namespace {

class HomePageUndoBubble : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(HomePageUndoBubble);
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

  // Called when the "undo" link is clicked.
  void UndoClicked();

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
  DCHECK(browser_);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::HOME_PAGE_UNDO);
}

HomePageUndoBubble::~HomePageUndoBubble() = default;

void HomePageUndoBubble::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  std::u16string undo_string =
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_BUBBLE_UNDO);
  std::vector<std::u16string> message = {
      l10n_util::GetStringUTF16(IDS_TOOLBAR_INFORM_SET_HOME_PAGE), undo_string};
  views::StyledLabel* label =
      AddChildView(std::make_unique<views::StyledLabel>());
  label->SetText(base::JoinString(message, base::StringPiece16(u" ")));

  gfx::Range undo_range(label->GetText().length() - undo_string.length(),
                        label->GetText().length());
  label->AddStyleRange(
      undo_range,
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &HomePageUndoBubble::UndoClicked, base::Unretained(this))));

  // Ensure StyledLabel has a cached size to return in GetPreferredSize().
  label->SizeToFit(0);
}

void HomePageUndoBubble::UndoClicked() {
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

BEGIN_METADATA(HomePageUndoBubble, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace


// HomeButton -----------------------------------------------------------

HomeButton::HomeButton(PressedCallback callback, Browser* browser)
    : ToolbarButton(std::move(callback)), browser_(browser) {
  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_MIDDLE_MOUSE_BUTTON);
  SetVectorIcons(kNavigateHomeIcon, kNavigateHomeTouchIcon);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_HOME));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME));
  SetID(VIEW_ID_HOME_BUTTON);
  SizeToPreferredSize();
}

HomeButton::~HomeButton() {
}

bool HomeButton::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = ui::OSExchangeData::URL;
  return true;
}

bool HomeButton::CanDrop(const OSExchangeData& data) {
  return data.HasURL(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
}

int HomeButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  return event.source_operations();
}

ui::mojom::DragOperation HomeButton::OnPerformDrop(
    const ui::DropTargetEvent& event) {
  if (!browser_)
    return ui::mojom::DragOperation::kNone;

  GURL new_homepage_url;
  std::u16string title;
  if (event.data().GetURLAndTitle(ui::FilenameToURLPolicy::CONVERT_FILENAMES,
                                  &new_homepage_url, &title) &&
      new_homepage_url.is_valid()) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);
    GURL old_homepage(prefs->GetString(prefs::kHomePage));

    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
    prefs->SetString(prefs::kHomePage, new_homepage_url.spec());

    HomePageUndoBubble::ShowBubble(browser_, old_is_ntp, old_homepage, this);
  }
  return ui::mojom::DragOperation::kNone;
}

BEGIN_METADATA(HomeButton, ToolbarButton)
END_METADATA
