// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace autofill {

namespace {

constexpr int kBetweenChildSpacing = 8;
constexpr int kBackgroundCornerRadius = 10;
constexpr int kBorderInsets = 12;
constexpr int kRowVerticalMargin = 8;
constexpr int kRowHorizontalMargin = 12;
constexpr int kMinimumWidth = 320;

}  // namespace

PopupPersonalContextNoticeView::PopupPersonalContextNoticeView(
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number,
    std::unique_ptr<PopupRowContentView> content_view)
    : PopupRowView(a11y_selection_delegate,
                   selection_delegate,
                   controller,
                   line_number,
                   std::move(content_view)),
      controller_(std::move(controller)),
      line_number_(line_number) {
  views::View& text_container = GetContentView();
  auto* layout_manager =
      static_cast<views::BoxLayout*>(text_container.GetLayoutManager());
  layout_manager->set_between_child_spacing(kBetweenChildSpacing);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  text_container.SetProperty(views::kMarginsKey, gfx::Insets());

  SetProperty(views::kMarginsKey,
              gfx::Insets::TLBR(kRowVerticalMargin, kRowHorizontalMargin,
                                kRowVerticalMargin, kRowHorizontalMargin));

  SetBackground(views::CreateRoundedRectBackground(
      ui::kColorSysSurface3, /*radius=*/kBackgroundCornerRadius));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kBorderInsets)));

  description_ =
      text_container.AddChildView(std::make_unique<views::StyledLabel>());

  layout_manager->SetFlexForView(description_, 1);

  std::u16string title_text = u"lorem ipsum ";
  std::u16string context_text = u"lorem ipsum ";
  std::u16string link_text = u"lorem ipsum";

  std::u16string full_text = title_text + context_text + link_text;
  const size_t full_text_length = full_text.length();
  GetViewAccessibility().SetName(full_text, ax::mojom::NameFrom::kAttribute);
  text_container.GetViewAccessibility().SetName(
      full_text, ax::mojom::NameFrom::kAttribute);
  description_->SetText(std::move(full_text));
  description_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);

  views::StyledLabel::RangeStyleInfo title_style;
  title_style.text_style = views::style::STYLE_BODY_5_MEDIUM;
  title_style.override_color_id = ui::kColorSysOnSurface;
  description_->AddStyleRange(gfx::Range(0, title_text.length()), title_style);

  views::StyledLabel::RangeStyleInfo context_style;
  context_style.text_style = views::style::STYLE_BODY_5_MEDIUM;
  context_style.override_color_id = ui::kColorLabelForegroundSecondary;
  description_->AddStyleRange(
      gfx::Range(title_text.length(),
                 title_text.length() + context_text.length()),
      context_style);

  size_t link_start = title_text.length() + context_text.length();
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PopupPersonalContextNoticeView::OnSettingsLinkClicked,
          base::Unretained(this)));
  description_->AddStyleRange(gfx::Range(link_start, full_text_length),
                              link_style);

  // TODO(crbug.com/517520354): Check dark theme appearance and accessibility.
  got_it_button_ =
      text_container.AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &PopupPersonalContextNoticeView::OnGotItButtonClicked,
              base::Unretained(this)),
          u"OK"));
  got_it_button_->SetStyle(ui::ButtonStyle::kTonal);
}

void PopupPersonalContextNoticeView::OnGotItButtonClicked() {
  if (controller_) {
    // TODO(crbug.com/520201413): Add metrics to track the cases when
    // `RemoveSuggestion` returns false.
    controller_->RemoveSuggestion(
        line_number_,
        AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked);
  }
}

void PopupPersonalContextNoticeView::OnSettingsLinkClicked() {
  if (!controller_ || !controller_->GetWebContents()) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(
      controller_->GetWebContents()->GetBrowserContext());
  if (!profile) {
    return;
  }
  chrome::ShowSettingsSubPageForProfile(profile,
                                        chrome::kSuggestionsFromGeminiSubPage);
}

views::Link* PopupPersonalContextNoticeView::GetSettingsLink() const {
  if (!description_) {
    return nullptr;
  }
  for (views::View* child : description_->children()) {
    if (views::IsViewClass<views::Link>(child)) {
      return views::AsViewClass<views::Link>(child);
    }
  }
  return nullptr;
}

void PopupPersonalContextNoticeView::Layout(views::View::PassKey pass_key) {
  LayoutSuperclass<PopupRowView>(this);

  // Because `description_` (a `StyledLabel`) creates its link child lazily
  // during layout, we must wait until after `LayoutSuperclass` runs to find
  // the link and set its focus behavior to `NEVER`. This prevents clicking the
  // link from stealing native focus from the search bar/input field.
  auto* link = GetSettingsLink();
  if (link) {
    link->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  }
}

gfx::Size PopupPersonalContextNoticeView::GetMinimumSize() const {
  return gfx::Size(kMinimumWidth, views::View::GetMinimumSize().height());
}

gfx::Size PopupPersonalContextNoticeView::CalculatePreferredSize(
    const views::SizeBounds& available_bounds) const {
  int target_width = available_bounds.width().is_bounded()
                         ? available_bounds.width().value()
                         : kMinimumWidth;

  target_width = std::max(kMinimumWidth, target_width);

  views::SizeBounds custom_bounds(target_width, available_bounds.height());
  gfx::Size size = views::View::CalculatePreferredSize(custom_bounds);
  size.set_width(target_width);
  return size;
}

PopupPersonalContextNoticeView::~PopupPersonalContextNoticeView() = default;

BEGIN_METADATA(PopupPersonalContextNoticeView)
END_METADATA

}  // namespace autofill
