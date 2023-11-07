// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
bool Cr2023ExpandedStateColorsEnabled() {
  return omnibox::IsOmniboxCr23CustomizeGuardedFeatureEnabled(
      omnibox::kExpandedStateColors);
}
}  // namespace

class OmniboxSuggestionRowButton : public views::MdTextButton {
 public:
  METADATA_HEADER(OmniboxSuggestionRowButton);
  OmniboxSuggestionRowButton(PressedCallback callback,
                             const std::u16string& text,
                             const gfx::VectorIcon& icon,
                             OmniboxPopupViewViews* popup_view,
                             OmniboxPopupSelection selection)
      : MdTextButton(std::move(callback), text, CONTEXT_OMNIBOX_PRIMARY),
        icon_(&icon),
        popup_view_(popup_view),
        selection_(selection) {
    SetTriggerableEventFlags(GetTriggerableEventFlags() |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    views::InstallPillHighlightPathGenerator(this);

    if (base::FeatureList::IsEnabled(omnibox::kCr2023ActionChips) ||
        features::GetChromeRefresh2023Level() ==
            features::ChromeRefresh2023Level::kLevel2) {
      SetImageLabelSpacing(8);
      SetCustomPadding(ChromeLayoutProvider::Get()->GetInsetsMetric(
          INSETS_OMNIBOX_PILL_BUTTON));
      SetCornerRadius(GetLayoutConstant(TOOLBAR_CORNER_RADIUS));
    } else {
      SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
      SetCustomPadding(ChromeLayoutProvider::Get()->GetInsetsMetric(
          INSETS_OMNIBOX_PILL_BUTTON));
      SetCornerRadius(GetInsets().height() +
                      GetLayoutConstant(LOCATION_BAR_ICON_SIZE));
    }

    auto* const ink_drop = views::InkDrop::Get(this);
    if (!Cr2023ExpandedStateColorsEnabled())
      ink_drop->SetHighlightOpacity(kOmniboxOpacityHovered);
    SetAnimationDuration(base::TimeDelta());
    ink_drop->GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());

    auto* const focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHasFocusPredicate(base::BindRepeating([](const View* view) {
      const auto* v = views::AsViewClass<OmniboxSuggestionRowButton>(view);
      CHECK(v);
      return v->GetVisible() && v->popup_view_->GetSelection() == v->selection_;
    }));
    focus_ring->SetColorId(kColorOmniboxResultsFocusIndicator);
  }

  OmniboxSuggestionRowButton(const OmniboxSuggestionRowButton&) = delete;
  OmniboxSuggestionRowButton& operator=(const OmniboxSuggestionRowButton&) =
      delete;

  ~OmniboxSuggestionRowButton() override = default;

  void SetThemeState(OmniboxPartState theme_state) {
    if (theme_state_ == theme_state)
      return;
    theme_state_ = theme_state;
    OnThemeChanged();
  }

  OmniboxPopupSelection selection() { return selection_; }

  void OnThemeChanged() override {
    MdTextButton::OnThemeChanged();
    // We can't use colors from NativeTheme as the omnibox theme might be
    // different (for example, if the NTP colors are customized).
    const auto* const color_provider = GetColorProvider();
    const bool selected = theme_state_ == OmniboxPartState::SELECTED;
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      *icon_,
                      selected ? kColorOmniboxResultsButtonIconSelected
                               : kColorOmniboxResultsButtonIcon,
                      GetLayoutConstant(LOCATION_BAR_ICON_SIZE)));
    SetEnabledTextColors(color_provider->GetColor(
        selected ? kColorOmniboxResultsTextSelected : kColorOmniboxText));
    if (Cr2023ExpandedStateColorsEnabled()) {
      ConfigureInkDropForRefresh2023(
          this,
          /*hover_color_id=*/
          selected ? kColorOmniboxResultsButtonInkDropRowSelected
                   : kColorOmniboxResultsButtonInkDropRowHovered,
          /*ripple_color_id=*/
          selected ? kColorOmniboxResultsButtonInkDropSelectedRowSelected
                   : kColorOmniboxResultsButtonInkDropSelectedRowHovered);
    } else {
      views::InkDrop::Get(this)->SetBaseColorId(
          selected ? kColorOmniboxResultsButtonInkDropSelected
                   : kColorOmniboxResultsButtonInkDrop);
    }

    views::FocusRing::Get(this)->SchedulePaint();
  }

  void UpdateBackgroundColor() override {
    const auto* const color_provider = GetColorProvider();
    const SkColor stroke_color =
        color_provider->GetColor(kColorOmniboxResultsButtonBorder);
    const SkColor fill_color =
        color_provider->GetColor(GetOmniboxBackgroundColorId(theme_state_));
    SetBackground(CreateBackgroundFromPainter(
        views::Painter::CreateRoundRectWith1PxBorderPainter(
            fill_color, stroke_color, GetCornerRadiusValue(),
            SkBlendMode::kSrcOver, /*antialias=*/true,
            /*should_border_scale=*/
            OmniboxFieldTrial::IsChromeRefreshActionChipShapeEnabled())));
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Although this appears visually as a button, expose as a list box option
    // so that it matches the other options within its list box container.
    node_data->role = ax::mojom::Role::kListBoxOption;
    node_data->SetName(GetAccessibleName());
  }

  void SetIcon(const gfx::VectorIcon& icon) {
    if (icon_ != &icon) {
      icon_ = &icon;
      OnThemeChanged();
    }
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  raw_ptr<OmniboxPopupViewViews> popup_view_;
  OmniboxPartState theme_state_ = OmniboxPartState::NORMAL;

  OmniboxPopupSelection selection_;
};

BEGIN_METADATA(OmniboxSuggestionRowButton, views::MdTextButton)
END_METADATA

OmniboxSuggestionButtonRowView::OmniboxSuggestionButtonRowView(
    OmniboxPopupViewViews* popup_view,
    int model_index)
    : popup_view_(popup_view), model_index_(model_index) {
  int left_margin = OmniboxMatchCellView::GetTextIndent();
  // +4 for the focus bar width, which shifts the suggest text but isn't
  // included in `GetTextIndent()`.
  if (OmniboxFieldTrial::IsCr23LayoutEnabled()) {
    left_margin += 4;
    // Do not apply left margin when action chips are inlined.
    if (OmniboxFieldTrial::IsActionsUISimplificationEnabled()) {
      left_margin = 0;
    }
  }
  int top_margin =
      OmniboxFieldTrial::IsChromeRefreshSuggestHoverFillShapeEnabled() ? 6 : 0;
  int bottom_margin =
      OmniboxFieldTrial::IsChromeRefreshSuggestHoverFillShapeEnabled()
          ? 6
          : ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING);
  const auto insets =
      gfx::Insets::TLBR(top_margin, left_margin, bottom_margin, 0);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetInteriorMargin(insets)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  BuildViews();

  if (OmniboxFieldTrial::IsChromeRefreshSuggestHoverFillShapeEnabled())
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);
}

void OmniboxSuggestionButtonRowView::BuildViews() {
  // Clear and reset existing views.
  {
    // Reset all raw_ptr instances first to avoid dangling.
    previous_active_button_ = nullptr;
    keyword_button_ = nullptr;
    action_buttons_.clear();

    RemoveAllChildViews();
  }

  // For all of these buttons, the visibility set from UpdateFromModel().
  // The Keyword and Pedal buttons also get their text from there, since the
  // text depends on the actual match. That shouldn't produce a flicker, because
  // it's called directly from OmniboxResultView::SetMatch(). If this flickers,
  // then so does everything else in the result view.
  {
    OmniboxPopupSelection selection(model_index_,
                                    OmniboxPopupSelection::KEYWORD_MODE);
    keyword_button_ = AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
        base::BindRepeating(&OmniboxSuggestionButtonRowView::ButtonPressed,
                            base::Unretained(this), selection),
        std::u16string(),
        OmniboxFieldTrial::IsChromeRefreshActionChipIconsEnabled()
            ? vector_icons::kSearchChromeRefreshIcon
            : vector_icons::kSearchIcon,
        popup_view_, selection));
  }

  if (!HasMatch()) {
    // Skip remaining code that depends on `match()`.
    return;
  }

  // Only create buttons for existent actions.
  for (size_t action_index = 0; action_index < match().actions.size();
       action_index++) {
    OmniboxPopupSelection selection(
        model_index_, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION,
        action_index);
    auto* button = AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
        base::BindRepeating(&OmniboxSuggestionButtonRowView::ButtonPressed,
                            base::Unretained(this), selection),
        std::u16string(), match().actions[action_index]->GetVectorIcon(),
        popup_view_, selection));
    action_buttons_.push_back(button);
  }
}

OmniboxSuggestionButtonRowView::~OmniboxSuggestionButtonRowView() = default;

void OmniboxSuggestionButtonRowView::Layout() {
  View::Layout();

  if (!OmniboxFieldTrial::IsChromeRefreshSuggestHoverFillShapeEnabled())
    return;

  auto bounds = GetLocalBounds();
  SkPath path;
  path.addRect(RectToSkRect(bounds), SkPathDirection::kCW, 0);
  SetClipPath(path);
}

void OmniboxSuggestionButtonRowView::UpdateFromModel() {
  if (!HasMatch()) {
    // Skip remaining code that depends on `match()`.
    return;
  }

  // Only build views if there was a structural change. Without this check,
  // performance could be impacted by frequent unnecessary rebuilds.
  if (action_buttons_.size() != match().actions.size()) {
    BuildViews();
  }

  // Used to keep track of which OmniboxSuggestionRowButton is the first in
  // the row, which can then be used to apply different layout/styling.
  OmniboxSuggestionRowButton* first_button = nullptr;

  SetPillButtonVisibility(keyword_button_, OmniboxPopupSelection::KEYWORD_MODE);
  if (keyword_button_->GetVisible()) {
    first_button = keyword_button_;

    std::u16string keyword;
    bool is_keyword_hint = false;
    match().GetKeywordUIState(
        popup_view_->controller()->client()->GetTemplateURLService(), &keyword,
        &is_keyword_hint);

    const auto names = SelectedKeywordView::GetKeywordLabelNames(
        keyword, popup_view_->controller()->client()->GetTemplateURLService());
    keyword_button_->SetText(names.full_name);
    keyword_button_->SetAccessibleName(
        l10n_util::GetStringFUTF16(IDS_ACC_KEYWORD_MODE, names.short_name));
  }

  for (const auto& action_button : action_buttons_) {
    SetPillButtonVisibility(action_button,
                            OmniboxPopupSelection::FOCUSED_BUTTON_ACTION);
    if (action_button->GetVisible()) {
      if (!first_button) {
        first_button = action_button;
      }

      const OmniboxAction* action =
          match().actions[action_button->selection().action_index].get();
      const auto label_strings = action->GetLabelStrings();
      action_button->SetText(label_strings.hint);
      action_button->SetTooltipText(label_strings.suggestion_contents);
      action_button->SetAccessibleName(label_strings.accessibility_hint);
      action_button->SetIcon(action->GetVectorIcon());
    }
  }

  if (OmniboxFieldTrial::IsActionsUISimplificationEnabled() && first_button) {
    // Apply a left margin of 4px (rather than zero) in order to make room for
    // the focus ring that gets rendered around action chips.
    first_button->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 4, 0,
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  }

  bool is_any_button_visible =
      keyword_button_->GetVisible() ||
      base::ranges::any_of(action_buttons_, [](const auto& action_button) {
        return action_button->GetVisible();
      });
  SetVisible(is_any_button_visible);
}

void OmniboxSuggestionButtonRowView::SelectionStateChanged() {
  auto* const active_button = GetActiveButton();
  if (active_button == previous_active_button_) {
    return;
  }
  if (previous_active_button_) {
    views::FocusRing::Get(previous_active_button_)->SchedulePaint();
  }
  if (active_button) {
    views::FocusRing::Get(active_button)->SchedulePaint();
  }
  previous_active_button_ = active_button;
}

void OmniboxSuggestionButtonRowView::SetThemeState(
    OmniboxPartState theme_state) {
  keyword_button_->SetThemeState(theme_state);
  for (const auto& action_button : action_buttons_) {
    action_button->SetThemeState(theme_state);
  }
}

views::Button* OmniboxSuggestionButtonRowView::GetActiveButton() const {
  std::vector<OmniboxSuggestionRowButton*> buttons{
      keyword_button_,
  };
  buttons.insert(buttons.end(), action_buttons_.begin(), action_buttons_.end());

  // Find the button that matches model selection.
  auto selected_button =
      base::ranges::find(buttons, popup_view_->GetSelection(),
                         &OmniboxSuggestionRowButton::selection);
  return selected_button == buttons.end() ? nullptr : *selected_button;
}

bool OmniboxSuggestionButtonRowView::HasMatch() const {
  return popup_view_->controller()->result().size() > model_index_;
}

const AutocompleteMatch& OmniboxSuggestionButtonRowView::match() const {
  return popup_view_->controller()->result().match_at(model_index_);
}

void OmniboxSuggestionButtonRowView::SetPillButtonVisibility(
    OmniboxSuggestionRowButton* button,
    OmniboxPopupSelection::LineState state) {
  button->SetVisible(popup_view_->model()->IsPopupControlPresentOnMatch(
      OmniboxPopupSelection(model_index_, state)));
}

void OmniboxSuggestionButtonRowView::ButtonPressed(
    const OmniboxPopupSelection selection,
    const ui::Event& event) {
  if (selection.state == OmniboxPopupSelection::KEYWORD_MODE) {
    // TODO(yoangela): Port to PopupModel and merge with keyEvent
    // TODO(orinj): Clear out existing suggestions, particularly this one, as
    // once we AcceptKeyword, we are really in a new scope state and holding
    // onto old suggestions is confusing and error prone. Without this check,
    // a second click of the button violates assumptions in |AcceptKeyword|.
    // Note: Since keyword mode logic depends on state of the edit model, the
    // selection must first be set to prepare for keyword mode before accepting.
    popup_view_->model()->SetPopupSelection(selection);
    // Don't re-enter keyword mode if already in it. This occurs when the user
    // was in keyword mode and re-clicked the same or a different keyword chip.
    if (popup_view_->model()->is_keyword_hint()) {
      const auto entry_method =
          event.IsMouseEvent() ? metrics::OmniboxEventProto::CLICK_HINT_VIEW
                               : metrics::OmniboxEventProto::TAP_HINT_VIEW;
      popup_view_->model()->AcceptKeyword(entry_method);
    }
  } else {
    WindowOpenDisposition disposition =
        ui::DispositionFromEventFlags(event.flags());
    popup_view_->model()->OpenSelection(selection, event.time_stamp(),
                                        disposition);
  }
}

BEGIN_METADATA(OmniboxSuggestionButtonRowView, views::View)
END_METADATA
