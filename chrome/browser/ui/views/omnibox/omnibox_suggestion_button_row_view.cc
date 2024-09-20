// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"

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
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

// A chip, like the history embeddings chip. Contains icon & text. Can not be
// focused or selected.
class OmniboxSuggestionRowChip : public views::MdTextButton {
  METADATA_HEADER(OmniboxSuggestionRowChip, views::MdTextButton)

 public:
  OmniboxSuggestionRowChip(const std::u16string& text,
                           const gfx::VectorIcon& icon)
      : MdTextButton({},
                     text,
                     CONTEXT_OMNIBOX_POPUP_ROW_CHIP,
                     /*use_text_color_for_icon=*/true),
        icon_(&icon) {
    // Default margin of the suggestion row's children is (0, 4, 0, 8). Remove
    // the left margin for chips per UX decision.
    SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0,
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

    SetImageLabelSpacing(5);
    SetCustomPadding(gfx::Insets::VH(0, 7));
    SetCornerRadius(100);  // Large number to ensure 100% rounded.

    views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(false);
  }

  OmniboxSuggestionRowChip(const OmniboxSuggestionRowChip&) = delete;
  OmniboxSuggestionRowChip& operator=(const OmniboxSuggestionRowChip&) = delete;

  ~OmniboxSuggestionRowChip() override = default;

  void SetThemeState(OmniboxPartState theme_state) {
    if (theme_state_ == theme_state)
      return;
    theme_state_ = theme_state;
    OnThemeChanged();
  }

  void OnThemeChanged() override {
    MdTextButton::OnThemeChanged();
    // We can't use colors from NativeTheme as the omnibox theme might be
    // different (for example, if the NTP colors are customized).
    const auto* const color_provider = GetColorProvider();
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*icon_, kColorOmniboxResultsIcon, 10));
    SetEnabledTextColors(
        color_provider->GetColor(ui::kColorSysOnSurfaceSubtle));
  }

  void UpdateBackgroundColor() override {
    const auto* const color_provider = GetColorProvider();
    const SkColor color =
        color_provider->GetColor(kColorOmniboxResultsChipBackground);
    SetBackground(
        views::CreateRoundedRectBackground(color, GetCornerRadiusValue()));
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  OmniboxPartState theme_state_ = OmniboxPartState::NORMAL;
};

BEGIN_METADATA(OmniboxSuggestionRowChip)
END_METADATA

// A button, like the switch-to-tab or keyword buttons. Contains icon & text.
// Can be focused and selected.
class OmniboxSuggestionRowButton : public views::MdTextButton {
  METADATA_HEADER(OmniboxSuggestionRowButton, views::MdTextButton)

 public:
  OmniboxSuggestionRowButton(PressedCallback callback,
                             const gfx::VectorIcon& icon,
                             OmniboxPopupViewViews* popup_view,
                             OmniboxPopupSelection selection)
      : MdTextButton(std::move(callback),
                     u"",
                     CONTEXT_OMNIBOX_PRIMARY,
                     /*use_text_color_for_icon=*/false),
        icon_(&icon),
        popup_view_(popup_view),
        selection_(selection) {
    SetTriggerableEventFlags(GetTriggerableEventFlags() |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    views::InstallPillHighlightPathGenerator(this);

    SetImageLabelSpacing(8);
    SetCustomPadding(ChromeLayoutProvider::Get()->GetInsetsMetric(
        INSETS_OMNIBOX_PILL_BUTTON));
    SetCornerRadius(GetLayoutConstant(TOOLBAR_CORNER_RADIUS));

    auto* const ink_drop = views::InkDrop::Get(this);
    SetAnimationDuration(base::TimeDelta());
    ink_drop->GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());

    auto* const focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHasFocusPredicate(base::BindRepeating([](const View* view) {
      const auto* v = views::AsViewClass<OmniboxSuggestionRowButton>(view);
      CHECK(v);
      return v->GetVisible() && v->popup_view_->GetSelection() == v->selection_;
    }));
    focus_ring->SetColorId(kColorOmniboxResultsFocusIndicator);

    GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
    GetViewAccessibility().SetIsSelected(false);
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
    ConfigureInkDropForRefresh2023(
        this,
        /*hover_color_id=*/
        selected ? kColorOmniboxResultsButtonInkDropRowSelected
                 : kColorOmniboxResultsButtonInkDropRowHovered,
        /*ripple_color_id=*/
        selected ? kColorOmniboxResultsButtonInkDropSelectedRowSelected
                 : kColorOmniboxResultsButtonInkDropSelectedRowHovered);

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
            /*should_border_scale=*/true)));
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

BEGIN_METADATA(OmniboxSuggestionRowButton)
END_METADATA

OmniboxSuggestionButtonRowView::OmniboxSuggestionButtonRowView(
    OmniboxPopupViewViews* popup_view,
    int model_index)
    : popup_view_(popup_view), model_index_(model_index) {
  const auto insets = gfx::Insets::TLBR(6, 0, 6, 0);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetInteriorMargin(insets)
      .SetDefault(
          views::kMarginsKey,
          // Set left margin to 4 instead of
          // `DISTANCE_RELATED_BUTTON_HORIZONTAL` (8) because there's already
          // built-in padding between the suggestion text and the button row.
          gfx::Insets::TLBR(0, 4, 0,
                            ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  BuildViews();

  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
}

void OmniboxSuggestionButtonRowView::BuildViews() {
  // Clear and reset existing views. Reset all raw_ptr instances first to avoid
  // dangling.
  previous_active_button_ = nullptr;
  embeddings_chip_ = nullptr;
  keyword_button_ = nullptr;
  action_buttons_.clear();
  RemoveAllChildViews();

  // Skip remaining code that depends on `match()`.
  if (!HasMatch())
    return;

  // For all of these buttons, the visibility is set from `UpdateFromModel()`.
  // The Keyword and Pedal buttons also get their text from there, since the
  // text depends on the actual match. That shouldn't produce a flicker, because
  // it's called directly from OmniboxResultView::SetMatch(). If this flickers,
  // then so does everything else in the result view.

  embeddings_chip_ = AddChildView(std::make_unique<OmniboxSuggestionRowChip>(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_EMBEDDING_HINT),
      omnibox::kSparkIcon));

  {
    OmniboxPopupSelection selection(model_index_,
                                    OmniboxPopupSelection::KEYWORD_MODE);
    keyword_button_ = AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
        base::BindRepeating(&OmniboxSuggestionButtonRowView::ButtonPressed,
                            base::Unretained(this), selection),
        vector_icons::kSearchChromeRefreshIcon, popup_view_, selection));
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
        match().actions[action_index]->GetVectorIcon(), popup_view_,
        selection));
    action_buttons_.push_back(button);
  }
}

OmniboxSuggestionButtonRowView::~OmniboxSuggestionButtonRowView() = default;

void OmniboxSuggestionButtonRowView::Layout(PassKey) {
  LayoutSuperclass<View>(this);

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

  embeddings_chip_->SetVisible(match().type ==
                               AutocompleteMatchType::HISTORY_EMBEDDINGS);

  if (match().HasInstantKeyword(
          popup_view_->controller()->client()->GetTemplateURLService())) {
    keyword_button_->SetVisible(false);
  } else {
    SetPillButtonVisibility(keyword_button_,
                            OmniboxPopupSelection::KEYWORD_MODE);
    if (keyword_button_->GetVisible()) {
      std::u16string keyword;
      std::u16string keyword_placeholder;
      bool is_keyword_hint = false;
      match().GetKeywordUIState(
          popup_view_->controller()->client()->GetTemplateURLService(),
          popup_view_->controller()->client()->IsHistoryEmbeddingsEnabled(),
          &keyword, &keyword_placeholder, &is_keyword_hint);

      const auto names = SelectedKeywordView::GetKeywordLabelNames(
          keyword,
          popup_view_->controller()->client()->GetTemplateURLService());
      keyword_button_->SetText(names.full_name);
      keyword_button_->GetViewAccessibility().SetName(
          l10n_util::GetStringFUTF16(IDS_ACC_KEYWORD_MODE, names.short_name));
    }
  }

  for (const auto& action_button : action_buttons_) {
    SetPillButtonVisibility(action_button,
                            OmniboxPopupSelection::FOCUSED_BUTTON_ACTION);
    if (action_button->GetVisible()) {
      const OmniboxAction* action =
          match().actions[action_button->selection().action_index].get();
      const auto label_strings = action->GetLabelStrings();
      action_button->SetText(label_strings.hint);
      action_button->SetTooltipText(label_strings.suggestion_contents);
      action_button->GetViewAccessibility().SetName(
          label_strings.accessibility_hint);
      action_button->SetIcon(action->GetVectorIcon());
    }
  }

  bool is_any_child_visible =
      embeddings_chip_->GetVisible() || keyword_button_->GetVisible() ||
      base::ranges::any_of(action_buttons_, [](const auto& action_button) {
        return action_button->GetVisible();
      });
  SetVisible(is_any_child_visible);
}

void OmniboxSuggestionButtonRowView::SelectionStateChanged() {
  auto* const active_button = GetActiveButton();
  if (active_button == previous_active_button_) {
    return;
  }
  if (previous_active_button_) {
    views::FocusRing::Get(previous_active_button_)->SchedulePaint();
    previous_active_button_->GetViewAccessibility().SetIsSelected(false);
  }
  if (active_button) {
    views::FocusRing::Get(active_button)->SchedulePaint();
    active_button->GetViewAccessibility().SetIsSelected(true);
  }
  previous_active_button_ = active_button;
}

void OmniboxSuggestionButtonRowView::SetThemeState(
    OmniboxPartState theme_state) {
  if (embeddings_chip_)
    embeddings_chip_->SetThemeState(theme_state);
  if (keyword_button_)
    keyword_button_->SetThemeState(theme_state);
  for (const auto& action_button : action_buttons_) {
    action_button->SetThemeState(theme_state);
  }
}

views::Button* OmniboxSuggestionButtonRowView::GetActiveButton() const {
  if (!HasMatch())
    return nullptr;

  std::vector<OmniboxSuggestionRowButton*> buttons{keyword_button_};
  buttons.insert(buttons.end(), action_buttons_.begin(), action_buttons_.end());

  // Find the button that matches model selection.
  auto selected_button =
      base::ranges::find(buttons, popup_view_->GetSelection(),
                         &OmniboxSuggestionRowButton::selection);
  return selected_button == buttons.end() ? nullptr : *selected_button;
}

bool OmniboxSuggestionButtonRowView::HasMatch() const {
  return popup_view_->controller()->autocomplete_controller()->result().size() >
         model_index_;
}

const AutocompleteMatch& OmniboxSuggestionButtonRowView::match() const {
  return popup_view_->controller()
      ->autocomplete_controller()
      ->result()
      .match_at(model_index_);
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

BEGIN_METADATA(OmniboxSuggestionButtonRowView)
END_METADATA
