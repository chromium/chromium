// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

class OmniboxSuggestionRowButton : public views::MdTextButton {
 public:
  OmniboxSuggestionRowButton(views::ButtonListener* listener,
                             const base::string16& text,
                             const gfx::VectorIcon& icon,
                             OmniboxPopupContentsView* popup_contents_view,
                             OmniboxPopupModel::Selection selection)
      : MdTextButton(listener, text, CONTEXT_OMNIBOX_PRIMARY),
        icon_(icon),
        popup_contents_view_(popup_contents_view),
        selection_(selection) {
    views::InstallPillHighlightPathGenerator(this);
    SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
    SetCustomPadding(ChromeLayoutProvider::Get()->GetInsetsMetric(
        INSETS_OMNIBOX_PILL_BUTTON));
    SetCornerRadius(GetInsets().height() +
                    GetLayoutConstant(LOCATION_BAR_ICON_SIZE));

    set_ink_drop_highlight_opacity(CalculateInkDropHighlightOpacity());
    focus_ring()->SetHasFocusPredicate([=](View* view) {
      return view->GetVisible() &&
             popup_contents_view_->model()->selection() == selection_;
    });
  }

  OmniboxSuggestionRowButton(const OmniboxSuggestionRowButton&) = delete;
  OmniboxSuggestionRowButton& operator=(const OmniboxSuggestionRowButton&) =
      delete;

  ~OmniboxSuggestionRowButton() override = default;

  SkColor GetInkDropBaseColor() const override {
    return color_utils::GetColorWithMaxContrast(background()->get_color());
  }

  void OnStyleRefresh() { focus_ring()->SchedulePaint(); }

  OmniboxPopupModel::Selection selection() { return selection_; }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    // MdTextButton uses custom colors when creating ink drop highlight.
    // We need the base implementation that uses GetInkDropBaseColor for
    // highlight.
    return views::InkDropHostView::CreateInkDropHighlight();
  }

  void OnThemeChanged() override {
    MdTextButton::OnThemeChanged();
    SkColor color =
        GetOmniboxColor(GetThemeProvider(), OmniboxPart::RESULTS_ICON,
                        OmniboxPartState::NORMAL);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(
                 icon_, GetLayoutConstant(LOCATION_BAR_ICON_SIZE), color));
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(GetAccessibleName());
    // Although this appears visually as a button, expose as a list box option
    // so that it matches the other options within its list box container.
    node_data->role = ax::mojom::Role::kListBoxOption;
  }

 private:
  const gfx::VectorIcon& icon_;
  OmniboxPopupContentsView* popup_contents_view_;
  OmniboxPopupModel::Selection selection_;

  float CalculateInkDropHighlightOpacity() {
    // Ink drop highlight opacity is result of mixing a layer with hovered
    // opacity and a layer with selected opacity. OmniboxPartState::SELECTED
    // opacity gets the same color as the selected omnibox row background (the
    // button would be the same color as the row) and overlaying it with
    // OmniboxPartState::HOVERED opacity makes the hovered button easily visible
    // in the selected or hovered row.
    return 1 - (1 - GetOmniboxStateOpacity(OmniboxPartState::HOVERED)) *
                   (1 - GetOmniboxStateOpacity(OmniboxPartState::SELECTED));
  }
};

OmniboxSuggestionButtonRowView::OmniboxSuggestionButtonRowView(
    OmniboxPopupContentsView* popup_contents_view,
    int model_index)
    : popup_contents_view_(popup_contents_view), model_index_(model_index) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetInteriorMargin(
          gfx::Insets(0, OmniboxMatchCellView::GetTextIndent(),
                      ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING),
                      0))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  // For all of these buttons, the visibility set from UpdateFromModel().
  // The Keyword and Pedal buttons also get their text from there, since the
  // text depends on the actual match. That shouldn't produce a flicker, because
  // it's called directly from OmniboxResultView::SetMatch(). If this flickers,
  // then so does everything else in the result view.
  keyword_button_ = AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
      this, base::string16(), vector_icons::kSearchIcon, popup_contents_view_,
      OmniboxPopupModel::Selection(model_index_,
                                   OmniboxPopupModel::FOCUSED_BUTTON_KEYWORD)));
  tab_switch_button_ =
      AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
          this, l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT),
          omnibox::kSwitchIcon, popup_contents_view_,
          OmniboxPopupModel::Selection(
              model_index_, OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH)));
  tab_switch_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACC_TAB_SWITCH_BUTTON));
  pedal_button_ = AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
      this, base::string16(), omnibox::kProductIcon, popup_contents_view_,
      OmniboxPopupModel::Selection(model_index_,
                                   OmniboxPopupModel::FOCUSED_BUTTON_PEDAL)));
}

OmniboxSuggestionButtonRowView::~OmniboxSuggestionButtonRowView() = default;

void OmniboxSuggestionButtonRowView::UpdateFromModel() {
  SetPillButtonVisibility(keyword_button_,
                          OmniboxPopupModel::FOCUSED_BUTTON_KEYWORD);
  if (keyword_button_->GetVisible()) {
    const OmniboxEditModel* edit_model = model()->edit_model();
    base::string16 keyword;
    bool is_keyword_hint = false;
    match().GetKeywordUIState(edit_model->client()->GetTemplateURLService(),
                              &keyword, &is_keyword_hint);

    const auto names = SelectedKeywordView::GetKeywordLabelNames(
        keyword, edit_model->client()->GetTemplateURLService());
    keyword_button_->SetText(names.full_name);
    keyword_button_->SetAccessibleName(
        l10n_util::GetStringFUTF16(IDS_ACC_KEYWORD_BUTTON, names.short_name));
  }

  SetPillButtonVisibility(pedal_button_,
                          OmniboxPopupModel::FOCUSED_BUTTON_PEDAL);
  if (pedal_button_->GetVisible()) {
    const auto pedal_strings = match().pedal->GetLabelStrings();
    pedal_button_->SetText(pedal_strings.hint);
    pedal_button_->SetTooltipText(pedal_strings.suggestion_contents);
    pedal_button_->SetAccessibleName(pedal_strings.accessibility_hint);
  }

  SetPillButtonVisibility(tab_switch_button_,
                          OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH);

  bool is_any_button_visible = keyword_button_->GetVisible() ||
                               pedal_button_->GetVisible() ||
                               tab_switch_button_->GetVisible();
  SetVisible(is_any_button_visible);
}

void OmniboxSuggestionButtonRowView::OnStyleRefresh() {
  keyword_button_->OnStyleRefresh();
  pedal_button_->OnStyleRefresh();
  tab_switch_button_->OnStyleRefresh();
}

void OmniboxSuggestionButtonRowView::ButtonPressed(views::Button* button,
                                                   const ui::Event& event) {
  OmniboxPopupModel* popup_model = popup_contents_view_->model();
  if (!popup_model)
    return;

  if (button == tab_switch_button_) {
    popup_model->TriggerSelectionAction(
        OmniboxPopupModel::Selection(
            model_index_, OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH),
        event.time_stamp());
  } else if (button == keyword_button_) {
    // TODO(yoangela): Port to PopupModel and merge with keyEvent
    // TODO(orinj): Clear out existing suggestions, particularly this one, as
    // once we AcceptKeyword, we are really in a new scope state and holding
    // onto old suggestions is confusing and error prone. Without this check,
    // a second click of the button violates assumptions in |AcceptKeyword|.
    if (model()->edit_model()->is_keyword_hint()) {
      auto method = metrics::OmniboxEventProto::INVALID;
      if (event.IsMouseEvent()) {
        method = metrics::OmniboxEventProto::CLICK_HINT_VIEW;
      } else if (event.IsGestureEvent()) {
        method = metrics::OmniboxEventProto::TAP_HINT_VIEW;
      }
      DCHECK_NE(method, metrics::OmniboxEventProto::INVALID);
      model()->edit_model()->AcceptKeyword(method);
    }
  } else if (button == pedal_button_) {
    popup_model->TriggerSelectionAction(
        OmniboxPopupModel::Selection(model_index_,
                                     OmniboxPopupModel::FOCUSED_BUTTON_PEDAL),
        event.time_stamp());
  }
}

views::Button* OmniboxSuggestionButtonRowView::GetActiveButton() const {
  std::vector<OmniboxSuggestionRowButton*> visible_buttons;
  if (keyword_button_->GetVisible())
    visible_buttons.push_back(keyword_button_);
  if (tab_switch_button_->GetVisible())
    visible_buttons.push_back(tab_switch_button_);
  if (pedal_button_->GetVisible())
    visible_buttons.push_back(pedal_button_);

  if (visible_buttons.empty())
    return nullptr;

  // Find first visible button that matches model selection.
  auto selected_button =
      std::find_if(visible_buttons.begin(), visible_buttons.end(),
                   [=](OmniboxSuggestionRowButton* button) {
                     return model()->selection() == button->selection();
                   });
  return selected_button == visible_buttons.end() ? visible_buttons.front()
                                                  : *selected_button;
}

const OmniboxPopupModel* OmniboxSuggestionButtonRowView::model() const {
  return popup_contents_view_->model();
}

const AutocompleteMatch& OmniboxSuggestionButtonRowView::match() const {
  return model()->result().match_at(model_index_);
}

void OmniboxSuggestionButtonRowView::SetPillButtonVisibility(
    OmniboxSuggestionRowButton* button,
    OmniboxPopupModel::LineState state) {
  button->SetVisible(model()->IsControlPresentOnMatch(
      OmniboxPopupModel::Selection(model_index_, state)));
}
