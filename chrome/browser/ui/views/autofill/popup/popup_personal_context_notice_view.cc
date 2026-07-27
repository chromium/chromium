// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/autofill/popup/popup_interactive_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace autofill {

namespace {

constexpr int kBetweenChildSpacing = 8;
constexpr int kBackgroundCornerRadius = 10;
constexpr int kBorderInsets = 12;
constexpr int kRowVerticalMargin = 12;
constexpr int kRowHorizontalMargin = 12;
constexpr int kMinimumWidth = 320;

// A border for link fragments that renders a 1px solid focus border without
// adding insets to the view content bounds. Setting non-zero insets on
// `views::Link` child views of `views::StyledLabel` reduces the content bounds
// width, causing `views::Label` to elide the link text.
class LinkFocusBorder : public views::Border {
 public:
  explicit LinkFocusBorder(bool is_focused) : is_focused_(is_focused) {
    SetColor(is_focused ? ui::kColorFocusableBorderFocused
                        : ui::ColorVariant());
  }

  LinkFocusBorder(const LinkFocusBorder&) = delete;
  LinkFocusBorder& operator=(const LinkFocusBorder&) = delete;
  ~LinkFocusBorder() override = default;

  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    if (is_focused_) {
      cc::PaintFlags flags;
      flags.setStrokeWidth(1.0f);
      flags.setColor(color().ResolveToSkColor(view.GetColorProvider()));
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setAntiAlias(true);

      gfx::RectF bounds(view.GetLocalBounds());
      bounds.Inset(0.5f);
      canvas->DrawRoundRect(bounds, /*radius=*/2.0f, flags);
    }
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(); }
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

 private:
  const bool is_focused_;
};

// TODO(b/524157152): Refactor AutofillPopupController to provide this.
bool IsLoggingDisabledByPolicy(const AutofillPopupController* controller) {
  if (!controller || !controller->GetWebContents()) {
    return false;
  }
  Profile* profile = Profile::FromBrowserContext(
      controller->GetWebContents()->GetBrowserContext());
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  const int policy_value = profile->GetPrefs()->GetInteger(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings);
  return policy_value ==
         std::to_underlying(
             optimization_guide::model_execution::prefs::
                 ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
}

}  // namespace

PopupPersonalContextNoticeView::PopupPersonalContextNoticeView(
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    base::RepeatingCallback<void(const std::u16string&, bool)>
        announce_callback,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : controller_(std::move(controller)),
      line_number_(line_number),
      announce_callback_(std::move(announce_callback)),
      a11y_selection_delegate_(a11y_selection_delegate) {
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kBetweenChildSpacing));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetProperty(views::kMarginsKey,
              gfx::Insets::TLBR(kRowVerticalMargin, kRowHorizontalMargin,
                                kRowVerticalMargin, kRowHorizontalMargin));

  SetBackground(views::CreateRoundedRectBackground(
      ui::kColorSysSurface3, /*radius=*/kBackgroundCornerRadius));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kBorderInsets)));

  description_ = AddChildView(std::make_unique<views::StyledLabel>());

  layout_manager->SetFlexForView(description_, 1);

  std::u16string title_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_TITLE);
  std::u16string context_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_CONTEXT);
  std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_LINK_TEXT);
  std::u16string button_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_OK_BUTTON);

  if (controller_) {
    if (controller_->GetMainFillingProduct() == FillingProduct::kAtMemory) {
      base::UmaHistogramEnumeration(
          "PersonalContext.AtMemory.NoticeInteractions",
          PersonalContextAtMemoryNoticeInteractions::kShown);
      if (!IsLoggingDisabledByPolicy(controller_.get())) {
        context_text = l10n_util::GetStringUTF16(
            IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_CONTEXT_WITH_LOGGING);
      }
    } else {
      base::UmaHistogramEnumeration(
          "PersonalContext.AmbientAutofill.NoticeInteractions",
          PersonalContextAmbientAutofillNoticeInteractions::kShown);
    }
  }

  // TODO(crbug.com/534738804): Use offset markers for string assembly to
  // support embedded links positioned before trailing text or punctuation.
  std::u16string full_text =
      base::JoinString({title_text, context_text, link_text}, u" ");
  const size_t full_text_length = full_text.length();
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(full_text, ax::mojom::NameFrom::kAttribute);
  description_->SetText(std::move(full_text));
  description_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  description_->SetDefaultTextStyle(views::style::STYLE_BODY_5);
  description_->SetLineHeight(views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_5));

  views::StyledLabel::RangeStyleInfo title_style;
  title_style.text_style = views::style::STYLE_BODY_5_MEDIUM;
  title_style.override_color_id = ui::kColorSysOnSurface;
  description_->AddStyleRange(gfx::Range(0, title_text.length()), title_style);

  views::StyledLabel::RangeStyleInfo context_style;
  context_style.text_style = views::style::STYLE_BODY_5;
  context_style.override_color_id = ui::kColorSysOnSurfaceSubtle;
  description_->AddStyleRange(
      gfx::Range(title_text.length() + 1,
                 title_text.length() + 1 + context_text.length()),
      context_style);

  size_t link_start = title_text.length() + 1 + context_text.length() + 1;
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PopupPersonalContextNoticeView::OnSettingsLinkClicked,
          base::Unretained(this)));
  link_style.text_style = views::style::STYLE_LINK_5;
  link_style.override_color_id = ui::kColorSysPrimary;
  description_->AddStyleRange(gfx::Range(link_start, full_text_length),
                              link_style);

  // TODO(crbug.com/517520354): Check dark theme appearance and accessibility.
  got_it_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PopupPersonalContextNoticeView::OnGotItButtonClicked,
                          base::Unretained(this)),
      button_text));
  got_it_button_->SetStyle(ui::ButtonStyle::kTonal);

  if (views::FocusRing* focus_ring = views::FocusRing::Get(got_it_button_)) {
    focus_ring->SetHasFocusPredicate(base::BindRepeating(
        [](const PopupPersonalContextNoticeView* notice_view,
           const views::View* view) { return notice_view->is_button_focused_; },
        base::Unretained(this)));
  }
}

std::optional<PopupInteractiveRowView::CellType>
PopupPersonalContextNoticeView::GetSelectedCell() const {
  return PopupInteractiveRowView::CellType::kContent;
}

void PopupPersonalContextNoticeView::SetSelectedCell(
    std::optional<PopupInteractiveRowView::CellType> cell) {
  if (cell) {
    FocusLink();
  } else {
    UnfocusLink();
    UnfocusButton();
  }
}

bool PopupPersonalContextNoticeView::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  // The main element (we always go through) is the "Settings" link and
  // the "Got it" button is the secondary one we can navigate to from it.
  const bool is_rtl = base::i18n::IsRTL();
  const int main_to_secondary = is_rtl ? ui::VKEY_LEFT : ui::VKEY_RIGHT;
  const int secondary_to_main = is_rtl ? ui::VKEY_RIGHT : ui::VKEY_LEFT;

  if (event.windows_key_code == main_to_secondary && is_link_focused_) {
    UnfocusLink();
    FocusButton();
    return true;
  } else if (event.windows_key_code == secondary_to_main &&
             is_button_focused_) {
    UnfocusButton();
    FocusLink();
    return true;
  }

  if (event.windows_key_code == ui::VKEY_RETURN) {
    if (is_link_focused_) {
      OnSettingsLinkClicked();
      return true;
    }
    if (is_button_focused_) {
      OnGotItButtonClicked();
      return true;
    }
  }

  return false;
}

bool PopupPersonalContextNoticeView::IsSelectable() const {
  return true;
}

void PopupPersonalContextNoticeView::OnGotItButtonClicked() {
  if (controller_) {
    if (controller_->GetMainFillingProduct() == FillingProduct::kAtMemory) {
      base::UmaHistogramEnumeration(
          "PersonalContext.AtMemory.NoticeInteractions",
          PersonalContextAtMemoryNoticeInteractions::kAcknowledged);
    } else {
      base::UmaHistogramEnumeration(
          "PersonalContext.AmbientAutofill.NoticeInteractions",
          PersonalContextAmbientAutofillNoticeInteractions::kAcknowledged);
    }
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

void PopupPersonalContextNoticeView::FocusLink() {
  if (description_) {
    is_link_focused_ = true;
    UpdateLinkBorders(/*focused=*/true);
    a11y_selection_delegate_->NotifyAXSelection(*this);
    GetViewAccessibility().NotifyEvent(ax::mojom::Event::kFocus, true);
    GetViewAccessibility().SetIsSelected(true);
    if (views::Link* link = GetSettingsLink()) {
      announce_callback_.Run(std::u16string(link->GetText()), /*polite=*/false);
    }
  }
}

void PopupPersonalContextNoticeView::UnfocusLink() {
  if (description_) {
    is_link_focused_ = false;
    UpdateLinkBorders(/*focused=*/false);
    GetViewAccessibility().SetIsSelected(false);
  }
}

void PopupPersonalContextNoticeView::UpdateLinkBorders(bool focused) {
  if (!description_) {
    return;
  }
  // A multi-line link created by `StyledLabel` is split into multiple link
  // fragments. We iterate over all `views::Link` child views to ensure the
  // focus border styling is applied to or removed from the entire wrapped link.
  for (views::View* child : description_->children()) {
    if (views::IsViewClass<views::Link>(child)) {
      child->SetBorder(std::make_unique<LinkFocusBorder>(focused));
    }
  }
}

void PopupPersonalContextNoticeView::FocusButton() {
  if (got_it_button_) {
    is_button_focused_ = true;
    got_it_button_->SetState(views::Button::STATE_HOVERED);
    if (views::FocusRing* focus_ring = views::FocusRing::Get(got_it_button_)) {
      focus_ring->Refresh();
    }
    a11y_selection_delegate_->NotifyAXSelection(*this);
    GetViewAccessibility().NotifyEvent(ax::mojom::Event::kFocus, true);
    GetViewAccessibility().SetIsSelected(true);
    announce_callback_.Run(std::u16string(got_it_button_->GetText()),
                           /*polite=*/false);
  }
}

void PopupPersonalContextNoticeView::UnfocusButton() {
  if (got_it_button_) {
    is_button_focused_ = false;
    got_it_button_->SetState(views::Button::STATE_NORMAL);
    if (views::FocusRing* focus_ring = views::FocusRing::Get(got_it_button_)) {
      focus_ring->Refresh();
    }
    GetViewAccessibility().SetIsSelected(false);
  }
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
  LayoutSuperclass<PopupInteractiveRowView>(this);

  // Because `description_` (a `StyledLabel`) creates its link child lazily
  // during layout, we must wait until after `LayoutSuperclass` runs to
  // configure the link. This sets its focus behavior to `NEVER` (preventing
  // clicking the link from stealing native focus from the search bar/input
  // field) and applies the active focus border styling.
  auto* link = GetSettingsLink();
  if (link) {
    link->SetFocusBehavior(views::View::FocusBehavior::NEVER);
    link->SetFontList(link->font_list().DeriveWithStyle(gfx::Font::UNDERLINE));
  }
  UpdateLinkBorders(is_link_focused_);
}

gfx::Size PopupPersonalContextNoticeView::GetMinimumSize() const {
  return gfx::Size(kMinimumWidth, views::View::GetMinimumSize().height());
}

gfx::Size PopupPersonalContextNoticeView::CalculatePreferredSize(
    const views::SizeBounds& available_bounds) const {
  // The notice is displayed inside a popup of width `kMinimumWidth`. Account
  // for this view's horizontal margins (`kRowHorizontalMargin` on each side)
  // to determine the maximum width actually available to this view.
  const int max_width = kMinimumWidth - 2 * kRowHorizontalMargin;
  int width = std::min(max_width,
                       available_bounds.width().value_or(max_width));
  width = std::max(0, width);

  // Ask the parent class for its preferred size given the available width.
  gfx::Size content_preferred_size =
      views::View::CalculatePreferredSize(views::SizeBounds(width, {}));

  return content_preferred_size;
}

PopupPersonalContextNoticeView::~PopupPersonalContextNoticeView() = default;

BEGIN_METADATA(PopupPersonalContextNoticeView)
END_METADATA

}  // namespace autofill
