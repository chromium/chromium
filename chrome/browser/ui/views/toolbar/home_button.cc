// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/home_button.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

// HomePageUndoBubble ---------------------------------------------------------

namespace {

class HomePageUndoBubble : public views::BubbleDialogDelegateView {
  METADATA_HEADER(HomePageUndoBubble, views::BubbleDialogDelegateView)

 public:
  HomePageUndoBubble(views::View* anchor_view,
                     PrefService* prefs,
                     const GURL& undo_url,
                     bool undo_value_is_ntp);
  HomePageUndoBubble(const HomePageUndoBubble&) = delete;
  HomePageUndoBubble& operator=(const HomePageUndoBubble&) = delete;
  ~HomePageUndoBubble() override = default;

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;

  // Called when the "undo" link is clicked.
  void UndoClicked();

  raw_ptr<PrefService> prefs_;
  GURL undo_url_;
  bool undo_value_is_ntp_;
};

HomePageUndoBubble::HomePageUndoBubble(views::View* anchor_view,
                                       PrefService* prefs,
                                       const GURL& undo_url,
                                       bool undo_value_is_ntp)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      prefs_(prefs),
      undo_url_(undo_url),
      undo_value_is_ntp_(undo_value_is_ntp) {
  DCHECK(prefs_);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
}

void HomePageUndoBubble::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  std::u16string undo_string =
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_BUBBLE_UNDO);
  std::vector<std::u16string> message = {
      l10n_util::GetStringUTF16(IDS_TOOLBAR_INFORM_SET_HOME_PAGE), undo_string};
  views::StyledLabel* label =
      AddChildView(std::make_unique<views::StyledLabel>());
  label->SetText(base::JoinString(message, std::u16string_view(u" ")));

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
  prefs_->SetString(prefs::kHomePage, undo_url_.spec());
  prefs_->SetBoolean(prefs::kHomePageIsNewTabPage, undo_value_is_ntp_);

  GetWidget()->Close();
}

BEGIN_METADATA(HomePageUndoBubble)
END_METADATA

}  // namespace

// HomePageUndoBubbleCoordinator ----------------------------------------------

HomePageUndoBubbleCoordinator::HomePageUndoBubbleCoordinator(
    views::View* anchor_view,
    PrefService* prefs)
    : anchor_view_(anchor_view), prefs_(prefs) {}

HomePageUndoBubbleCoordinator::~HomePageUndoBubbleCoordinator() = default;

void HomePageUndoBubbleCoordinator::Show(const GURL& undo_url,
                                         bool undo_value_is_ntp) {
  if (tracker_.view())
    tracker_.view()->GetWidget()->Close();

  auto undo_bubble = std::make_unique<HomePageUndoBubble>(
      anchor_view_, prefs_, undo_url, undo_value_is_ntp);
  tracker_.SetView(undo_bubble.get());
  views::BubbleDialogDelegateView::CreateBubble(std::move(undo_bubble))->Show();
}

// HomeButton -----------------------------------------------------------------

HomeButton::HomeButton(PressedCallback callback, PrefService* prefs)
    : ToolbarButton(std::move(callback)),
      prefs_(prefs),
      coordinator_(this, prefs) {
  SetProperty(views::kElementIdentifierKey, kToolbarHomeButtonElementId);
  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_MIDDLE_MOUSE_BUTTON);
  SetVectorIcons(kNavigateHomeChromeRefreshIcon, kNavigateHomeTouchIcon);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_HOME));
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME));
  SetID(VIEW_ID_HOME_BUTTON);
  SizeToPreferredSize();
}

HomeButton::~HomeButton() = default;

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

views::View::DropCallback HomeButton::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&HomeButton::UpdateHomePage,
                        weak_ptr_factory_.GetWeakPtr());
}

void HomeButton::UpdateHomePage(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  std::optional<ui::OSExchangeData::UrlInfo> url_info =
      event.data().GetURLAndTitle(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
  if (url_info.has_value() && url_info->url.is_valid() && prefs_) {
    GURL old_homepage(prefs_->GetString(prefs::kHomePage));
    bool old_is_ntp = prefs_->GetBoolean(prefs::kHomePageIsNewTabPage);

    prefs_->SetString(prefs::kHomePage, url_info->url.spec());
    prefs_->SetBoolean(prefs::kHomePageIsNewTabPage, false);

    coordinator_.Show(old_homepage, old_is_ntp);
  }
  output_drag_op = ui::mojom::DragOperation::kNone;
}

BEGIN_METADATA(HomeButton)
END_METADATA
