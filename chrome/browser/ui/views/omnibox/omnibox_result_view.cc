// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <limits.h>

#include <algorithm>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/remove_suggestion_bubble.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view_class_properties.h"

#if defined(OS_WIN)
#include "base/win/atl.h"
#endif

namespace {

class OmniboxRemoveSuggestionButton : public views::ImageButton {
 public:
  METADATA_HEADER(OmniboxRemoveSuggestionButton);
  explicit OmniboxRemoveSuggestionButton(PressedCallback callback)
      : ImageButton(std::move(callback)) {
    views::ConfigureVectorImageButton(this);

    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(
        l10n_util::GetStringUTF16(IDS_ACC_REMOVE_SUGGESTION_BUTTON));
    // Although this appears visually as a button, expose as a list box option
    // so that it matches the other options within its list box container.
    node_data->role = ax::mojom::Role::kListBoxOption;
  }
};

BEGIN_METADATA(OmniboxRemoveSuggestionButton, views::ImageButton)
END_METADATA

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultSelectionIndicator

class OmniboxResultSelectionIndicator : public views::View {
 public:
  METADATA_HEADER(OmniboxResultSelectionIndicator);

  static constexpr int kStrokeThickness = 3;

  explicit OmniboxResultSelectionIndicator(OmniboxResultView* result_view)
      : result_view_(result_view) {
    SetPreferredSize({kStrokeThickness, 0});
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    SkPath path = GetPath();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(path, flags);
  }

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    color_ = result_view_->GetColor(OmniboxPart::RESULTS_FOCUS_BAR);
  }

 private:
  SkColor color_;

  // Pointer to the parent view.
  OmniboxResultView* const result_view_;

  // The focus bar is a straight vertical line with half-rounded endcaps. Since
  // this geometry is nontrivial to represent using primitives, it's instead
  // represented using a fill path. This matches the style and implementation
  // used in Tab Groups.
  SkPath GetPath() const {
    SkPath path;

    path.moveTo(0, 0);
    path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
               SkPathDirection::kCW, kStrokeThickness, kStrokeThickness);
    path.lineTo(kStrokeThickness, height() - kStrokeThickness);
    path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
               SkPathDirection::kCW, 0, height());
    path.close();

    return path;
  }
};

BEGIN_METADATA(OmniboxResultSelectionIndicator, views::View)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, public:

OmniboxResultView::OmniboxResultView(
    OmniboxPopupContentsView* popup_contents_view,
    size_t model_index)
    : AnimationDelegateViews(this),
      popup_contents_view_(popup_contents_view),
      model_index_(model_index),
      keyword_slide_animation_(new gfx::SlideAnimation(this)),
      // Using base::Unretained is correct here. 'this' outlives the callback.
      mouse_enter_exit_handler_(
          base::BindRepeating(&OmniboxResultView::UpdateHoverState,
                              base::Unretained(this))) {
  CHECK_GE(model_index, 0u);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  suggestion_container_ = AddChildView(std::make_unique<views::View>());
  suggestion_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  mouse_enter_exit_handler_.ObserveMouseEnterExitOn(suggestion_container_);

  if (OmniboxFieldTrial::IsRefinedFocusStateEnabled()) {
    // TODO(olesiamarukhno): Consider making it a decoration instead of separate
    // view (painting it in a layer).
    selection_indicator_ = suggestion_container_->AddChildView(
        std::make_unique<OmniboxResultSelectionIndicator>(this));
  }

  views::View* suggestion_button_container =
      suggestion_container_->AddChildView(std::make_unique<views::View>());
  suggestion_button_container
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  suggestion_button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  suggestion_view_ = suggestion_button_container->AddChildView(
      std::make_unique<OmniboxMatchCellView>(this));
  suggestion_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(4));

  const gfx::Insets child_insets(0, 0, 0, OmniboxMatchCellView::kMarginRight);
  suggestion_tab_switch_button_ = suggestion_button_container->AddChildView(
      std::make_unique<OmniboxTabSwitchButton>(
          base::BindRepeating(&OmniboxResultView::ButtonPressed,
                              base::Unretained(this),
                              OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH),
          popup_contents_view_, this,
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT),
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_SHORT_HINT),
          omnibox::kSwitchIcon));
  suggestion_tab_switch_button_->SetProperty(views::kMarginsKey, child_insets);
  suggestion_tab_switch_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(OmniboxTabSwitchButton::GetFlexRule())
          .WithWeight(1));

  // This is intentionally not in the tab order by default, but should be if the
  // user has full-acessibility mode on. This is because this is a tertiary
  // priority button, which already has a Shift+Delete shortcut.
  // TODO(tommycli): Make sure we announce the Shift+Delete capability in the
  // accessibility node data for removable suggestions.
  remove_suggestion_button_ = suggestion_button_container->AddChildView(
      std::make_unique<OmniboxRemoveSuggestionButton>(base::BindRepeating(
          &OmniboxResultView::ButtonPressed, base::Unretained(this),
          OmniboxPopupModel::FOCUSED_BUTTON_REMOVE_SUGGESTION)));
  remove_suggestion_button_->SetProperty(views::kMarginsKey, child_insets);
  views::InstallCircleHighlightPathGenerator(remove_suggestion_button_);
  remove_suggestion_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_REMOVE_SUGGESTION));
  remove_suggestion_focus_ring_ =
      views::FocusRing::Install(remove_suggestion_button_);
  remove_suggestion_focus_ring_->SetHasFocusPredicate([&](View* view) {
    return view->GetVisible() && GetMatchSelected() &&
           (popup_contents_view_->model()->selected_line_state() ==
            OmniboxPopupModel::FOCUSED_BUTTON_REMOVE_SUGGESTION);
  });

  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    button_row_ = AddChildView(std::make_unique<OmniboxSuggestionButtonRowView>(
        popup_contents_view_, model_index));

    // Quickly mouse-exiting through the suggestion button row sometimes leaves
    // the whole row highlighted. This fixes that. It doesn't seem necessary to
    // further observe the child controls of |button_row_|.
    mouse_enter_exit_handler_.ObserveMouseEnterExitOn(button_row_);
  }

  keyword_view_ = suggestion_button_container->AddChildView(
      std::make_unique<OmniboxMatchCellView>(this));
  keyword_view_->SetVisible(false);
  keyword_view_->icon()->SetFlipCanvasOnPaintForRTLUI(true);
  keyword_view_->icon()->SizeToPreferredSize();
}

OmniboxResultView::~OmniboxResultView() {}

// static
std::unique_ptr<views::Background> OmniboxResultView::GetPopupCellBackground(
    views::View* view,
    OmniboxPartState part_state) {
  DCHECK(view);

  bool prefers_contrast = view->GetNativeTheme() &&
                          view->GetNativeTheme()->UserHasContrastPreference();
  // TODO(tapted): Consider using background()->SetNativeControlColor() and
  // always have a background.
  if ((part_state == OmniboxPartState::NORMAL && !prefers_contrast))
    return nullptr;

  return views::CreateSolidBackground(GetOmniboxColor(
      view->GetThemeProvider(), OmniboxPart::RESULTS_BACKGROUND, part_state));
}

SkColor OmniboxResultView::GetColor(OmniboxPart part) const {
  return GetOmniboxColor(GetThemeProvider(), part, GetThemeState());
}

void OmniboxResultView::SetMatch(const AutocompleteMatch& match) {
  match_ = match.GetMatchWithContentsAndDescriptionPossiblySwapped();
  keyword_slide_animation_->Reset();

  const int suggestion_indent =
      popup_contents_view_->InExplicitExperimentalKeywordMode() ? 70 : 0;
  suggestion_view_->SetProperty(views::kMarginsKey,
                                gfx::Insets(0, suggestion_indent, 0, 0));

  suggestion_view_->OnMatchUpdate(this, match_);
  keyword_view_->OnMatchUpdate(this, match_);
  suggestion_tab_switch_button_->SetVisible(ShouldShowTabMatchButtonInline());
  UpdateRemoveSuggestionVisibility();

  suggestion_view_->content()->SetTextWithStyling(match_.contents,
                                                  match_.contents_class);
  if (match_.answer) {
    suggestion_view_->content()->AppendExtraText(match_.answer->first_line());
    suggestion_view_->description()->SetTextWithStyling(
        match_.answer->second_line(), true);
  } else {
    const bool deemphasize =
        match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
        match_.type == AutocompleteMatchType::PEDAL;
    suggestion_view_->description()->SetTextWithStyling(
        match_.description, match_.description_class, deemphasize);
  }

  // |keyword_view_| only needs to be updated if the keyword search button is
  // not enabled.
  if (!OmniboxFieldTrial::IsKeywordSearchButtonEnabled()) {
    AutocompleteMatch* keyword_match = match_.associated_keyword.get();
    keyword_view_->SetVisible(keyword_match != nullptr);
    if (keyword_match) {
      keyword_view_->content()->SetTextWithStyling(
          keyword_match->contents, keyword_match->contents_class);
      keyword_view_->description()->SetTextWithStyling(
          keyword_match->description, keyword_match->description_class);
    }
  }
  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    button_row_->UpdateFromModel();
  }

  ApplyThemeAndRefreshIcons();
  SetWidths();
}

void OmniboxResultView::ShowKeywordSlideAnimation(bool show_keyword) {
  if (show_keyword)
    keyword_slide_animation_->Show();
  else
    keyword_slide_animation_->Hide();
}

void OmniboxResultView::ApplyThemeAndRefreshIcons(bool force_reapply_styles) {
  SetBackground(GetPopupCellBackground(this, GetThemeState()));

  // Reapply the dim color to account for the highlight state.
  suggestion_view_->separator()->ApplyTextColor(
      OmniboxPart::RESULTS_TEXT_DIMMED);
  keyword_view_->separator()->ApplyTextColor(OmniboxPart::RESULTS_TEXT_DIMMED);
  if (suggestion_tab_switch_button_->GetVisible())
    suggestion_tab_switch_button_->UpdateBackground();
  if (remove_suggestion_button_->GetVisible())
    remove_suggestion_focus_ring_->SchedulePaint();

  // Recreate the icons in case the color needs to change.
  // Note: if this is an extension icon or favicon then this can be done in
  //       SetMatch() once (rather than repeatedly, as happens here). There may
  //       be an optimization opportunity here.
  // TODO(dschuyler): determine whether to optimize the color changes.
  suggestion_view_->icon()->SetImage(GetIcon().ToImageSkia());
  keyword_view_->icon()->SetImage(gfx::CreateVectorIcon(
      omnibox::kKeywordSearchIcon, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColor(OmniboxPart::RESULTS_ICON)));

  // We must reapply colors for all the text fields here. If we don't, we can
  // break theme changes for ZeroSuggest. See https://crbug.com/1095205.
  //
  // TODO(tommycli): We should finish migrating this logic to live entirely
  // within OmniboxTextView, which should keep track of its own OmniboxPart.
  bool prefers_contrast =
      GetNativeTheme() && GetNativeTheme()->UserHasContrastPreference();
  if (match_.answer) {
    suggestion_view_->content()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DEFAULT);
    suggestion_view_->description()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DEFAULT);
  } else if (match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
             match_.type == AutocompleteMatchType::PEDAL) {
    suggestion_view_->content()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DEFAULT);
    suggestion_view_->description()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DIMMED);
  } else if (prefers_contrast || force_reapply_styles) {
    // Normally, OmniboxTextView caches its appearance, but in high contrast,
    // selected-ness changes the text colors, so the styling of the text part of
    // the results needs to be recomputed.
    suggestion_view_->content()->ReapplyStyling();
    suggestion_view_->description()->ReapplyStyling();
  }

  if (force_reapply_styles) {
    keyword_view_->content()->ReapplyStyling();
    keyword_view_->description()->ReapplyStyling();
  } else if (keyword_view_->GetVisible()) {
    keyword_view_->description()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DIMMED);
  }

  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    button_row_->OnOmniboxBackgroundChange(GetOmniboxColor(
        GetThemeProvider(), OmniboxPart::RESULTS_BACKGROUND, GetThemeState()));
  }

  if (OmniboxFieldTrial::IsRefinedFocusStateEnabled()) {
    // The focus bar indicates when the suggestion is focused. Do not show the
    // focus bar if an auxiliary button is selected.
    selection_indicator_->SetVisible(
        GetMatchSelected() &&
        popup_contents_view_->model()->selected_line_state() ==
            OmniboxPopupModel::NORMAL);
  }
}

void OmniboxResultView::OnSelectionStateChanged() {
  UpdateRemoveSuggestionVisibility();
  if (GetMatchSelected()) {
    // Immediately before notifying screen readers that the selected item has
    // changed, we want to update the name of the newly-selected item so that
    // any cached values get updated prior to the selection change.
    EmitTextChangedAccessiblityEvent();

    auto selection_state = popup_contents_view_->model()->selection().state;

    // The text is also accessible via text/value change events in the omnibox
    // but this selection event allows the screen reader to get more details
    // about the list and the user's position within it.
    // Limit which selection states fire the events, in order to avoid duplicate
    // events. Specifically, OmniboxPopupContentsView::ProvideButtonFocusHint()
    // already fires the correct events when the user tabs to an attached button
    // in the current row.
    if (selection_state == OmniboxPopupModel::FOCUSED_BUTTON_HEADER ||
        selection_state == OmniboxPopupModel::NORMAL) {
      popup_contents_view_->FireAXEventsForNewActiveDescendant(this);
    }

    // The slide animation is not used in the new suggestion button row UI.
    ShowKeywordSlideAnimation(
        !OmniboxFieldTrial::IsKeywordSearchButtonEnabled() &&
        selection_state == OmniboxPopupModel::KEYWORD_MODE);
  } else {
    ShowKeywordSlideAnimation(false);
  }
  ApplyThemeAndRefreshIcons();
}

bool OmniboxResultView::GetMatchSelected() const {
  // The header button being focused means the match itself is NOT focused.
  return popup_contents_view_->GetSelectedIndex() == model_index_ &&
         popup_contents_view_->model()->selected_line_state() !=
             OmniboxPopupModel::FOCUSED_BUTTON_HEADER;
}

views::Button* OmniboxResultView::GetActiveAuxiliaryButtonForAccessibility() {
  if (popup_contents_view_->model()->selected_line_state() ==
      OmniboxPopupModel::FOCUSED_BUTTON_REMOVE_SUGGESTION) {
    return remove_suggestion_button_;
  }

  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    return button_row_->GetActiveButton();
  } else if (popup_contents_view_->model()->selected_line_state() ==
             OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH) {
    return suggestion_tab_switch_button_;
  }

  return nullptr;
}

OmniboxPartState OmniboxResultView::GetThemeState() const {
  if (GetMatchSelected())
    return OmniboxPartState::SELECTED;

  // If we don't highlight the whole row when the user has the mouse over the
  // remove suggestion button, it's unclear which suggestion is being removed.
  // That does not apply to the tab switch button, which is much larger.
  bool highlight_row =
      IsMouseHovered() && !suggestion_tab_switch_button_->IsMouseHovered();
  return highlight_row ? OmniboxPartState::HOVERED : OmniboxPartState::NORMAL;
}

void OmniboxResultView::OnMatchIconUpdated() {
  // The new icon will be fetched during ApplyThemeAndRefreshIcons().
  ApplyThemeAndRefreshIcons();
}

void OmniboxResultView::SetRichSuggestionImage(const gfx::ImageSkia& image) {
  suggestion_view_->SetImage(image);
}

void OmniboxResultView::ButtonPressed(OmniboxPopupModel::LineState state,
                                      const ui::Event& event) {
  popup_contents_view_->model()->TriggerSelectionAction(
      OmniboxPopupModel::Selection(model_index_, state), event.time_stamp());
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides:

bool OmniboxResultView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    popup_contents_view_->SetSelectedIndex(model_index_);
  return true;
}

bool OmniboxResultView::OnMouseDragged(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location())) {
    // When the drag enters or remains within the bounds of this view, either
    // set the state to be selected or hovered, depending on the mouse button.
    if (event.IsOnlyLeftMouseButton()) {
      if (!GetMatchSelected())
        popup_contents_view_->SetSelectedIndex(model_index_);
      if (suggestion_tab_switch_button_) {
        gfx::Point point_in_child_coords(event.location());
        View::ConvertPointToTarget(this, suggestion_tab_switch_button_,
                                   &point_in_child_coords);
        if (suggestion_tab_switch_button_->HitTestPoint(
                point_in_child_coords)) {
          SetMouseAndGestureHandler(suggestion_tab_switch_button_);
          return false;
        }
      }
    } else {
      UpdateHoverState();
    }
    return true;
  }

  // When the drag leaves the bounds of this view, cancel the hover state and
  // pass control to the popup view.
  UpdateHoverState();
  SetMouseAndGestureHandler(popup_contents_view_);
  return false;
}

void OmniboxResultView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() || event.IsOnlyLeftMouseButton()) {
    WindowOpenDisposition disposition =
        event.IsOnlyLeftMouseButton()
            ? WindowOpenDisposition::CURRENT_TAB
            : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    popup_contents_view_->OpenMatch(model_index_, disposition,
                                    event.time_stamp());
  }
}

void OmniboxResultView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHoverState();
}

void OmniboxResultView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHoverState();
}

void OmniboxResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Get the label without the ", n of m" positional text appended.
  // The positional info is provided via
  // ax::mojom::IntAttribute::kPosInSet/SET_SIZE and providing it via text as
  // well would result in duplicate announcements.
  // Pass false for |is_tab_switch_button_focused|, because the button will
  // receive its own label in the case that a screen reader is listening to
  // selection events on items rather than announcements or value change events.

  // TODO(tommycli): We re-fetch the original match from the popup model,
  // because |match_| already has its contents and description swapped by this
  // class, and we don't want that for the bubble. We should improve this.
  bool is_selected = GetMatchSelected();
  OmniboxPopupModel* model = popup_contents_view_->model();
  if (model_index_ < model->result().size()) {
    AutocompleteMatch raw_match = model->result().match_at(model_index_);
    // The selected match can have a special name, e.g. when is one or more
    // buttons that can be tabbed to.
    std::u16string label =
        is_selected ? model->GetAccessibilityLabelForCurrentSelection(
                          raw_match.contents, false)
                    : AutocompleteMatchType::ToAccessibilityLabel(
                          raw_match, raw_match.contents);
    node_data->SetName(label);
  }

  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             model_index_ + 1);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             model->result().size());

  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, is_selected);
  if (IsMouseHovered())
    node_data->AddState(ax::mojom::State::kHovered);
}


void OmniboxResultView::OnThemeChanged() {
  views::View::OnThemeChanged();
  views::SetImageFromVectorIconWithColor(
      remove_suggestion_button_, vector_icons::kCloseRoundedIcon,
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColor(OmniboxPart::RESULTS_ICON));
  ApplyThemeAndRefreshIcons(true);
}

void OmniboxResultView::EmitTextChangedAccessiblityEvent() {
  if (!popup_contents_view_->IsOpen())
    return;

  // The omnibox results list reuses the same items, but the text displayed for
  // these items is updated as the value of omnibox changes. The displayed text
  // for a given item is exposed to screen readers as the item's name/label.
  ui::AXNodeData node_data;
  GetAccessibleNodeData(&node_data);
  std::u16string current_name =
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
  if (accessible_name_ != current_name) {
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
    accessible_name_ = current_name;
  }
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, private:

gfx::Image OmniboxResultView::GetIcon() const {
  return popup_contents_view_->GetMatchIcon(
      match_, GetColor(OmniboxPart::RESULTS_ICON));
}

void OmniboxResultView::UpdateHoverState() {
  UpdateRemoveSuggestionVisibility();
  ApplyThemeAndRefreshIcons();
}

bool OmniboxResultView::ShouldShowTabMatchButtonInline() {
  return !OmniboxFieldTrial::IsSuggestionButtonRowEnabled() &&
         popup_contents_view_->model()->IsControlPresentOnMatch(
             OmniboxPopupModel::Selection(
                 model_index_, OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH));
}

void OmniboxResultView::UpdateRemoveSuggestionVisibility() {
  bool old_visibility = remove_suggestion_button_->GetVisible();
  bool new_visibility =
      popup_contents_view_->model()->IsControlPresentOnMatch(
          OmniboxPopupModel::Selection(
              model_index_,
              OmniboxPopupModel::FOCUSED_BUTTON_REMOVE_SUGGESTION)) &&
      (GetMatchSelected() || IsMouseHovered());

  remove_suggestion_button_->SetVisible(new_visibility);

  if (old_visibility != new_visibility)
    InvalidateLayout();
}

void OmniboxResultView::SetWidths() {
  // TODO(pkasting): Use an animating layout manager
  const int min_keyword_width =
      std::min(OmniboxMatchCellView::GetTextIndent(), width());
  keyword_view_->SetPreferredSize(
      {keyword_slide_animation_->CurrentValueBetween(min_keyword_width,
                                                     width()),
       keyword_view_->CalculatePreferredSize().height()});

  InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides, private:

void OmniboxResultView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  keyword_slide_animation_->SetSlideDuration(
      base::TimeDelta::FromMilliseconds(width() / 4));
  SetWidths();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::AnimationDelegateViews overrides, private:

void OmniboxResultView::AnimationProgressed(const gfx::Animation* animation) {
  SetWidths();
}

DEFINE_ENUM_CONVERTERS(OmniboxPartState,
                       {OmniboxPartState::NORMAL, u"NORMAL"},
                       {OmniboxPartState::HOVERED, u"HOVERED"},
                       {OmniboxPartState::SELECTED, u"SELECTED"})

BEGIN_METADATA(OmniboxResultView, views::View)
ADD_READONLY_PROPERTY_METADATA(bool, MatchSelected)
ADD_READONLY_PROPERTY_METADATA(OmniboxPartState, ThemeState)
ADD_READONLY_PROPERTY_METADATA(gfx::Image, Icon)
END_METADATA
