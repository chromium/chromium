// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

namespace {

std::unique_ptr<views::StyledLabel> CreateBodyLabel(std::u16string& body_text) {
  return views::Builder<views::StyledLabel>()
      .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
      .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
      .SetText(body_text)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .Build();
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kPriceTrackingBubbleDialogId);

PriceTrackingBubbleDialogView::PriceTrackingBubbleDialogView(
    View* anchor_view,
    content::WebContents* web_contents,
    Profile* profile,
    const GURL& url,
    ui::ImageModel image_model,
    OnTrackPriceCallback on_track_price_callback,
    Type type)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      profile_(profile),
      url_(url),
      type_(type) {
  SetProperty(views::kElementIdentifierKey, kPriceTrackingBubbleDialogId);
  SetShowCloseButton(true);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
             static_cast<int>(ui::mojom::DialogButton::kOk));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  auto folder_name = commerce::GetBookmarkParentName(
      BookmarkModelFactory::GetForBrowserContext(profile_), url);

  if (type == PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE) {
    SetTitle(l10n_util::GetStringUTF16(
        IDS_OMNIBOX_TRACK_PRICE_DIALOG_TITLE_FIRST_RUN));
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACK_PRICE_DIALOG_ACTION_BUTTON));
    SetButtonLabel(ui::mojom::DialogButton::kCancel,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACK_PRICE_DIALOG_CANCEL_BUTTON));
    SetAcceptCallback(base::BindOnce(&PriceTrackingBubbleDialogView::OnAccepted,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(on_track_price_callback)));
    SetCancelCallback(base::BindOnce(&PriceTrackingBubbleDialogView::OnCanceled,
                                     weak_factory_.GetWeakPtr(),
                                     base::DoNothing()));
    auto body_text = l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_TRACK_PRICE_DIALOG_DESCRIPTION_FIRST_RUN,
        folder_name.value_or(std::u16string()));
    body_label_ = AddChildView(CreateBodyLabel(body_text));
  } else if (type == PriceTrackingBubbleDialogView::Type::TYPE_NORMAL) {
    SetTitle(
        l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE_DIALOG_TITLE));
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACKING_PRICE_DIALOG_ACTION_BUTTON));
    SetButtonLabel(ui::mojom::DialogButton::kCancel,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACKING_PRICE_DIALOG_UNTRACK_BUTTON));
    SetAcceptCallback(base::BindOnce(&PriceTrackingBubbleDialogView::OnAccepted,
                                     weak_factory_.GetWeakPtr(),
                                     base::DoNothing()));
    SetCancelCallback(base::BindOnce(&PriceTrackingBubbleDialogView::OnCanceled,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(on_track_price_callback)));

    const bookmarks::BookmarkNode* bookmark =
        BookmarkModelFactory::GetForBrowserContext(profile_)
            ->GetMostRecentlyAddedUserNodeForURL(url);

    auto body_text =
        l10n_util::GetStringUTF16(IDS_PRICE_TRACKING_SAVE_DESCRIPTION);

    // If there is a bookmark, add the location and link text.
    if (bookmark) {
      auto bookmark_location = l10n_util::GetStringFUTF16(
          IDS_PRICE_TRACKING_SAVE_LOCATION, folder_name.value());
      body_text += u" " + bookmark_location;
    }

    body_label_ = AddChildView(CreateBodyLabel(body_text));

    // Only set up the bookmark link if there was a bookmark for the URL.
    if (bookmark) {
      // Offset the period at the end by minus one.
      int32_t offset = body_text.length() - folder_name->length() - 1;
      views::StyledLabel::RangeStyleInfo style_info =
          views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
              &PriceTrackingBubbleDialogView::ShowBookmarkEditor,
              weak_factory_.GetWeakPtr()));
      body_label_->AddStyleRange(
          gfx::Range(offset, offset + folder_name->length()), style_info);
    }
  }
  SetMainImage(std::move(image_model));
  body_label_->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
}

PriceTrackingBubbleDialogView::~PriceTrackingBubbleDialogView() = default;

void PriceTrackingBubbleDialogView::ShowBookmarkEditor() {
  DCHECK(anchor_widget());

  gfx::NativeWindow native_parent = anchor_widget()->GetNativeWindow();
  DCHECK(native_parent);

  GetWidget()->Close();

  const bookmarks::BookmarkNode* node =
      BookmarkModelFactory::GetForBrowserContext(profile_)
          ->GetMostRecentlyAddedUserNodeForURL(url_);

  if (node && native_parent) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.EditedBookmarkFolderFromOmniboxBubble"));

    BookmarkEditor::Show(native_parent, profile_,
                         BookmarkEditor::EditDetails::EditNode(node),
                         BookmarkEditor::SHOW_TREE);
  }
}

void PriceTrackingBubbleDialogView::OnAccepted(
    OnTrackPriceCallback on_track_price_callback) {
  if (type_ == PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.FirstRunBubbleTrackedPrice"));
  }

  std::move(on_track_price_callback).Run(true);
}

void PriceTrackingBubbleDialogView::OnCanceled(
    OnTrackPriceCallback on_track_price_callback) {
  if (type_ == PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.FirstRunBubbleDismissed"));
  } else if (type_ == PriceTrackingBubbleDialogView::Type::TYPE_NORMAL) {
    base::RecordAction(
        base::UserMetricsAction("Commerce.PriceTracking.Confirmation.Untrack"));
  }
  std::move(on_track_price_callback).Run(false);
}

BEGIN_METADATA(PriceTrackingBubbleDialogView)
END_METADATA

// PriceTrackingBubbleCoordinator
PriceTrackingBubbleCoordinator::PriceTrackingBubbleCoordinator(
    views::View* anchor_view)
    : anchor_view_(anchor_view) {}

PriceTrackingBubbleCoordinator::~PriceTrackingBubbleCoordinator() = default;

void PriceTrackingBubbleCoordinator::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(bubble_widget_observation_.IsObservingSource(widget));
  bubble_widget_observation_.Reset();

  std::move(on_dialog_closing_callback_).Run();
}

void PriceTrackingBubbleCoordinator::Show(
    content::WebContents* web_contents,
    Profile* profile,
    const GURL& url,
    ui::ImageModel image_model,
    PriceTrackingBubbleDialogView::OnTrackPriceCallback callback,
    base::OnceClosure on_dialog_closing_callback,
    PriceTrackingBubbleDialogView::Type type) {
  DCHECK(!tracker_.view());
  on_dialog_closing_callback_ = std::move(on_dialog_closing_callback);

  if (type == PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE) {
    base::RecordAction(
        base::UserMetricsAction("Commerce.PriceTracking.FirstRunBubbleShown"));
  } else if (type == PriceTrackingBubbleDialogView::Type::TYPE_NORMAL) {
    base::RecordAction(
        base::UserMetricsAction("Commerce.PriceTracking.ConfirmationShown"));
  }

  auto bubble = std::make_unique<PriceTrackingBubbleDialogView>(
      anchor_view_, web_contents, profile, url, std::move(image_model),
      std::move(callback), type);
  tracker_.SetView(bubble.get());
  auto* widget = PriceTrackingBubbleDialogView::CreateBubble(std::move(bubble));
  bubble_widget_observation_.Observe(widget);
  widget->Show();
}

void PriceTrackingBubbleCoordinator::Hide() {
  if (IsShowing()) {
    tracker_.view()->GetWidget()->Close();
  }
  tracker_.SetView(nullptr);
}

PriceTrackingBubbleDialogView* PriceTrackingBubbleCoordinator::GetBubble()
    const {
  return tracker_.view() ? views::AsViewClass<PriceTrackingBubbleDialogView>(
                               const_cast<views::View*>(tracker_.view()))
                         : nullptr;
}

bool PriceTrackingBubbleCoordinator::IsShowing() {
  return tracker_.view() != nullptr;
}
