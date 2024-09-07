// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_email_dialog_view.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kPriceTrackingEmailConsentDialogId);

namespace {

// The margin separating the two paragraphs in this dialog.
constexpr int kParagraphMargin = 8;

const char kPriceTrackingHelpLink[] =
    "https://support.google.com/chrome/?p=price_tracking_desktop";

std::unique_ptr<views::StyledLabel> CreateBodyLabel(std::u16string& body_text) {
  return views::Builder<views::StyledLabel>()
      .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
      .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
      .SetText(body_text)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .Build();
}

}  // namespace

PriceTrackingEmailDialogView::PriceTrackingEmailDialogView(
    View* anchor_view,
    content::WebContents* web_contents,
    Profile* profile)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      profile_(profile) {
  SetShowCloseButton(true);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width=*/true));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
             static_cast<int>(ui::mojom::DialogButton::kOk));

  int bubble_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  set_fixed_width(bubble_width);

  SetTitle(l10n_util::GetStringUTF16(IDS_PRICE_TRACKING_EMAIL_CONSENT_TITLE));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_PRICE_TRACKING_YES_IM_IN));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_PRICE_TRACKING_NOT_NOW));
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
  SetAcceptCallback(base::BindOnce(&PriceTrackingEmailDialogView::OnAccepted,
                                   weak_factory_.GetWeakPtr()));
  SetCancelCallback(base::BindOnce(&PriceTrackingEmailDialogView::OnCanceled,
                                   weak_factory_.GetWeakPtr()));
  SetCloseCallback(base::BindOnce(&PriceTrackingEmailDialogView::OnClosed,
                                  weak_factory_.GetWeakPtr()));

  CoreAccountInfo account_info =
      IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync);

  auto email = base::UTF8ToUTF16(account_info.email);

  auto learn_more_text = l10n_util::GetStringUTF16(
      IDS_PRICE_TRACKING_EMAIL_CONSENT_LEARN_MORE_LINK_TEXT);

  auto body_text =
      l10n_util::GetStringFUTF16(IDS_PRICE_TRACKING_EMAIL_CONSENT_BODY, email);

  body_label_ = AddChildView(CreateBodyLabel(body_text));
  body_label_->SizeToFit(bubble_width);
  body_label_->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(kParagraphMargin, 0));

  int32_t email_offset = body_text.find(email);
  views::StyledLabel::RangeStyleInfo email_style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&PriceTrackingEmailDialogView::OpenSettings,
                              weak_factory_.GetWeakPtr()));
  body_label_->AddStyleRange(
      gfx::Range(email_offset, email_offset + email.length()),
      email_style_info);

  help_label_ = AddChildView(CreateBodyLabel(learn_more_text));
  help_label_->SizeToFit(bubble_width);

  views::StyledLabel::RangeStyleInfo link_style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&PriceTrackingEmailDialogView::OpenHelpArticle,
                              weak_factory_.GetWeakPtr()));
  help_label_->AddStyleRange(gfx::Range(0, learn_more_text.length()),
                             link_style_info);

  body_label_->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  help_label_->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
}

PriceTrackingEmailDialogView::~PriceTrackingEmailDialogView() = default;

void PriceTrackingEmailDialogView::OpenHelpArticle() {
  // Open web page with help article.
  web_contents()->OpenURL(
      content::OpenURLParams(GURL(kPriceTrackingHelpLink), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false),
      /*navigation_handle_callback=*/{});
  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.EmailConsentDialog.HelpLinkClicked"));
}

void PriceTrackingEmailDialogView::OpenSettings() {
  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.EmailConsentDialog.EmailLinkClicked"));
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile_);
  chrome::ShowSettings(browser_displayer.browser());
}

void PriceTrackingEmailDialogView::OnAccepted() {
  // The user enabled price tracking emails, the preference will be updated and
  // we won't show this dialog again.
  if (profile_ && profile_->GetPrefs()) {
    profile_->GetPrefs()->SetBoolean(commerce::kPriceEmailNotificationsEnabled,
                                     true);
  }

  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.EmailConsentDialog.Accepted"));
}

void PriceTrackingEmailDialogView::OnCanceled() {
  // The user redjected the offer of price tracking emails. The preference will
  // be set and we won't show this dialog again.
  if (profile_ && profile_->GetPrefs()) {
    profile_->GetPrefs()->SetBoolean(commerce::kPriceEmailNotificationsEnabled,
                                     false);
  }

  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.EmailConsentDialog.Rejected"));
}

void PriceTrackingEmailDialogView::OnClosed() {
  // The user dismissed the dialog without taking an explicit action. We'll
  // leave the preference unchanged and show this dialog again when appropriate.
  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.EmailConsentDialog.Closed"));

  // Make sure to report that the dialog was closed to the IPH system.
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  if (tracker) {
    tracker->Dismissed(
        feature_engagement::kIPHPriceTrackingEmailConsentFeature);
  }
}

BEGIN_METADATA(PriceTrackingEmailDialogView)
END_METADATA

// PriceTrackingEmailDialogCoordinator
PriceTrackingEmailDialogCoordinator::PriceTrackingEmailDialogCoordinator(
    views::View* anchor_view)
    : anchor_view_(anchor_view) {}

PriceTrackingEmailDialogCoordinator::~PriceTrackingEmailDialogCoordinator() =
    default;

void PriceTrackingEmailDialogCoordinator::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK(bubble_widget_observation_.IsObservingSource(widget));
  bubble_widget_observation_.Reset();

  std::move(on_dialog_closing_callback_).Run();
}

void PriceTrackingEmailDialogCoordinator::Show(
    content::WebContents* web_contents,
    Profile* profile,
    base::OnceClosure on_dialog_closing_callback) {
  DCHECK(!tracker_.view());
  on_dialog_closing_callback_ = std::move(on_dialog_closing_callback);

  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.EmailConsentDialog.Shown"));

  auto bubble = std::make_unique<PriceTrackingEmailDialogView>(
      anchor_view_, web_contents, profile);
  bubble->SetProperty(views::kElementIdentifierKey,
                      kPriceTrackingEmailConsentDialogId);
  tracker_.SetView(bubble.get());
  auto* widget = PriceTrackingEmailDialogView::CreateBubble(std::move(bubble));
  bubble_widget_observation_.Observe(widget);
  widget->Show();
}

void PriceTrackingEmailDialogCoordinator::Hide() {
  if (IsShowing()) {
    tracker_.view()->GetWidget()->Close();
  }
  tracker_.SetView(nullptr);
}

PriceTrackingEmailDialogView* PriceTrackingEmailDialogCoordinator::GetBubble()
    const {
  return tracker_.view() ? views::AsViewClass<PriceTrackingEmailDialogView>(
                               const_cast<views::View*>(tracker_.view()))
                         : nullptr;
}

bool PriceTrackingEmailDialogCoordinator::IsShowing() {
  return tracker_.view() != nullptr;
}
