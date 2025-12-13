// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
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
#include "ui/gfx/geometry/size.h"
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
#include "ui/views/layout/layout_manager.h"
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
    if (theme_state_ == theme_state) {
      return;
    }
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
    SetBackground(views::CreateRoundedRectBackground(color, GetCornerRadii()));
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  OmniboxPartState theme_state_ = OmniboxPartState::NORMAL;
};

BEGIN_METADATA(OmniboxSuggestionRowChip)
END_METADATA

class OmniboxSuggestionButtonRowLayout : public views::LayoutManager {
 public:
  OmniboxSuggestionButtonRowLayout() = default;
  ~OmniboxSuggestionButtonRowLayout() override = default;

  gfx::Size GetMinimumSize(const views::View* host) const override {
    return CalculateSize(
        host, [](const views::View* v) { return v->GetMinimumSize(); });
  }

  gfx::Size GetPreferredSize(const views::View* host) const override {
    return CalculateSize(
        host, [](const views::View* v) { return v->GetPreferredSize({}); });
  }

  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override {
    return GetPreferredSize(host);
  }

  // This layout is similar to a `FlexLayout` with `kPreferredSnapToZero`, with
  // a priority to give children their preferred size from left to right. With
  // the additional constraint that once there is not enough space to expand a
  // child to its preferred size, all remaining children after it are forced to
  // be displayed at their minimum sizes. The goal is to collapse the children
  // to their minimum sizes from right to left as the available space decreases,
  // strictly in that order.
  //
  // If there is space to show all children at their preferred sizes, the layout
  // will look like this:
  // +--------------------------------------------------------------+
  // |  preferred_size_1  |  preferred_size_2  |  preferred_size_3  |
  // +--------------------------------------------------------------+
  // But as the available space decreases, the layout will look like this:
  // +---------------------------------------------------+
  // |  preferred_size_1  |  preferred_size_2  |  min_3  |
  // +---------------------------------------------------+
  // And then this:
  // +----------------------------------------+
  // |  preferred_size_1  |  min_2  |  min_3  |
  // +----------------------------------------+
  // And finally this:
  // +-----------------------------+
  // |  min_1  |  min_2  |  min_3  |
  // +-----------------------------+
  //
  // `FlexLayout` cannot be used because, if there is room to expand any of the
  // children, it will always do so regardless of child order or weight values.
  // Which leads to the following undesired behavior, where the children are
  // collapsed from right to left, according their order values:
  // +--------------------------------------------+
  // |  big_preferred_size_1  |  min_2  |  min_3  |
  // +--------------------------------------------+
  // But then when there is not enough space to show the first child at its
  // preferred size, if the child in the middle has a smaller preferred size
  // than the first child, it is expanded to fill the remaining space, resulting
  // in the following:
  // +----------------------------------------+
  // |  min_1  |  preferred_size_2  |  min_3  |
  // +----------------------------------------+
  // Which violates our requirement to strictly collapse in order, from right to
  // left.
  void Layout(views::View* host) override {
    const gfx::Size& host_size = host->bounds().size();

    // The total width used is, at a minimum, the minimum width of the host
    // view, which is the sum of the minimum widths of all children plus insets.
    // In the loop below, we will accumulate any additional width required to
    // show children at their preferred width, for those children that are
    // allowed to expand.
    int total_width_used = GetMinimumSize(host).width();

    const int y = row_insets_.top() + button_insets_.top();
    int current_x = row_insets_.left();
    bool allow_expansion = true;
    // This loop positions the visible children one by one, checking to see if
    // the additional width required to display them at their preferred width is
    // available. If it is, the child is allocated its preferred width with
    // `SetBoundsRect()`. If not, the child is allocated its minimum width and
    // `allow_expansion` is set to false to force the remaining children to also
    // be allocated only their minimum widths.
    for (views::View* child : host->children()) {
      if (!child->GetVisible()) {
        continue;
      }

      const gfx::Size& child_minimum_size = child->GetMinimumSize();
      const gfx::Size& child_preferred_size = child->GetPreferredSize({});
      int additional_width_for_preferred =
          child_preferred_size.width() - child_minimum_size.width();
      // This is the key element of logic for this layout. Here we check to see
      // if all the width used so far plus the additional width for this child's
      // preferred width is less than or equal to the total width available (the
      // host width).
      bool preferred_width_available =
          total_width_used + additional_width_for_preferred <=
          host_size.width();
      allow_expansion = allow_expansion && preferred_width_available;
      int child_allocated_width = allow_expansion ? child_preferred_size.width()
                                                  : child_minimum_size.width();

      child->SetBoundsRect(
          {{current_x + button_insets_.left(), y},
           {child_allocated_width, child_preferred_size.height()}});
      total_width_used += allow_expansion ? additional_width_for_preferred : 0;
      current_x += child_allocated_width + button_insets_.width();
    }
  }

 private:
  // Both `GetMinimumSize()` and `GetPreferredSize()` require identical logic,
  // but based on the host's children's minimum or preferred sizes,
  // respectively. This function captures that common logic and accepts a lambda
  // to get the minimum or preferred size from each child.
  template <typename ChildSizeGetter>
  gfx::Size CalculateSize(const views::View* host,
                          ChildSizeGetter get_child_size) const {
    const auto& children = host->children();

    int width = 0;
    int height = 0;
    // Accumulate the total width and biggest height of all visible children.
    for (const views::View* child : children) {
      if (!child->GetVisible()) {
        continue;
      }
      const gfx::Size child_size = get_child_size(child);
      width += child_size.width() + button_insets_.width();
      height = std::max(height, child_size.height() + button_insets_.height());
    }
    // Add in the button row insets.
    // NOTE: This logic does not include any option for the "collapse margins"
    // behavior that FlexLayout offers. If it's required in the future, it could
    // be added here.
    width += row_insets_.width();
    height += row_insets_.height();

    return {width, height};
  }

  // TODO(crbug.com/422549792): These insets need to be adjusted to be larger on
  //   right and left in order to show more space when the suggestion text/view
  //   is not visible. Which will then require coordination with the insets of
  //   the suggestion text/view. Additionally, the values here were selected to
  //   produce the designed layout, but they should either be explained further
  //   here or specified with `ChromeLayoutProvider`.
  gfx::Insets row_insets_ = gfx::Insets::TLBR(6, 4, 6, 0);
  gfx::Insets button_insets_ =
      gfx::Insets::TLBR(0,
                        0,
                        0,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
};

// A button, like the switch-to-tab or keyword buttons. Contains icon & text.
// Can be focused and selected.
class OmniboxSuggestionRowButton : public views::MdTextButton {
  METADATA_HEADER(OmniboxSuggestionRowButton, views::MdTextButton)

 public:
  OmniboxSuggestionRowButton(PressedCallback callback,
                             int context,
                             const gfx::VectorIcon& icon,
                             const gfx::Image& image,
                             OmniboxPopupViewViews* popup_view,
                             OmniboxPopupSelection selection)
      : MdTextButton(std::move(callback),
                     u"",
                     context,
                     /*use_text_color_for_icon=*/false),
        icon_(&icon),
        image_(image),
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

  // Suggestion row buttons are allowed to collapse to the size of their icons
  // and insets only. Whether they are allocated their preferred size, which
  // also includes the label, is controlled by OmniboxSuggestionButtonRowLayout.
  gfx::Size GetMinimumSize() const override {
    gfx::Size size = image_container_view()->GetPreferredSize({});
    size.Enlarge(GetInsets().width(), GetInsets().height());
    return size;
  }

  void SetThemeState(OmniboxPartState theme_state) {
    if (theme_state_ == theme_state) {
      return;
    }
    theme_state_ = theme_state;
    OnThemeChanged();
  }

  OmniboxPopupSelection selection() const { return selection_; }

  void OnThemeChanged() override {
    MdTextButton::OnThemeChanged();
    // We can't use colors from NativeTheme as the omnibox theme might be
    // different (for example, if the NTP colors are customized).
    const auto* const color_provider = GetColorProvider();
    const bool selected = theme_state_ == OmniboxPartState::SELECTED;
    if (!image_.IsEmpty()) {
      SetImageModel(views::Button::STATE_NORMAL,
                    ui::ImageModel::FromImage(image_));
    } else {
      SetImageModel(views::Button::STATE_NORMAL,
                    ui::ImageModel::FromVectorIcon(
                        *icon_,
                        selected ? kColorOmniboxResultsButtonIconSelected
                                 : kColorOmniboxResultsButtonIcon,
                        GetLayoutConstant(LOCATION_BAR_ICON_SIZE)));
    }
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
            fill_color, stroke_color, GetCornerRadii(), SkBlendMode::kSrcOver,
            /*antialias=*/true,
            /*should_border_scale=*/true)));
  }

  void SetIcon(const gfx::VectorIcon& icon) {
    if (icon_ != &icon) {
      icon_ = &icon;
      OnThemeChanged();
    }
  }

  void SetImage(const gfx::Image& image) {
    if (image_ != image) {
      image_ = image;
      OnThemeChanged();
    }
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  gfx::Image image_;
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
  SetLayoutManager(std::make_unique<OmniboxSuggestionButtonRowLayout>());
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
  if (!HasMatch()) {
    return;
  }

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
        CONTEXT_OMNIBOX_PRIMARY, vector_icons::kSearchChromeRefreshIcon,
        gfx::Image(), popup_view_, selection));
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
        match().IsToolbelt() ? CONTEXT_OMNIBOX_TOOLBELT_BUTTON
                             : CONTEXT_OMNIBOX_PRIMARY,
        match().actions[action_index]->GetVectorIcon(),
        match().actions[action_index]->GetIconImage(), popup_view_, selection));
    action_buttons_.push_back(button);
  }
}

OmniboxSuggestionButtonRowView::~OmniboxSuggestionButtonRowView() = default;

void OmniboxSuggestionButtonRowView::Layout(PassKey) {
  LayoutSuperclass<View>(this);

  const auto bounds = GetLocalBounds();
  SetClipPath(SkPath::Rect(RectToSkRect(bounds)));
}

void OmniboxSuggestionButtonRowView::UpdateFromModel() {
  if (!HasMatch()) {
    // Skip remaining code that depends on `match()`.
    return;
  }

  // Only build views if there was a structural change. Without this check,
  // performance could be impacted by frequent unnecessary rebuilds.
  // Note, the toolbelt is always rebuilt because its action count is mostly
  // constant but selection changes frequently invalidate button focus states.
  // TODO(crbug.com/429029560): try removing this optimization or making the
  // conditions smarter.
  if (action_buttons_.size() != match().actions.size() ||
      (match().IsToolbelt() &&
       omnibox_feature_configs::Toolbelt::Get().rebuild_button_row_views)) {
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
      CHECK(!match().associated_keyword.empty());
      const auto names = SelectedKeywordView::GetKeywordLabelNames(
          match().associated_keyword,
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
      if (!action->GetIconImage().IsEmpty()) {
        action_button->SetImage(action->GetIconImage());
      } else {
        action_button->SetIcon(action->GetVectorIcon());
      }
    }
  }

  // Clear focus. Otherwise, an updated button might show the focus ring if the
  // button its replacing was previously focused.
  if (previous_active_button_) {
    views::FocusRing::Get(previous_active_button_)->SchedulePaint();
    previous_active_button_->GetViewAccessibility().SetIsSelected(false);
    previous_active_button_ = nullptr;
  }

  const bool is_any_child_visible =
      embeddings_chip_->GetVisible() || keyword_button_->GetVisible() ||
      std::ranges::any_of(action_buttons_, [](const auto& action_button) {
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
  if (embeddings_chip_) {
    embeddings_chip_->SetThemeState(theme_state);
  }
  if (keyword_button_) {
    keyword_button_->SetThemeState(theme_state);
  }
  for (const auto& action_button : action_buttons_) {
    action_button->SetThemeState(theme_state);
  }
}

views::Button* OmniboxSuggestionButtonRowView::GetActiveButton() const {
  if (!HasMatch()) {
    return nullptr;
  }

  std::vector<OmniboxSuggestionRowButton*> buttons{keyword_button_};
  buttons.insert(buttons.end(), action_buttons_.begin(), action_buttons_.end());

  // Find the button that matches model selection.
  auto selected_button =
      std::ranges::find(buttons, popup_view_->GetSelection(),
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
  button->SetVisible(
      popup_view_->controller()->edit_model()->IsPopupControlPresentOnMatch(
          OmniboxPopupSelection(model_index_, state)));
}

void OmniboxSuggestionButtonRowView::ButtonPressed(
    const OmniboxPopupSelection selection,
    const ui::Event& event) {
  if (selection.state == OmniboxPopupSelection::KEYWORD_MODE) {
    // Note: Since keyword mode logic depends on state of the edit model, the
    // selection must first be set to prepare for keyword mode before accepting.
    popup_view_->controller()->edit_model()->SetPopupSelection(selection);
    // Don't re-enter keyword mode if already in it. This occurs when the user
    // was in keyword mode and re-clicked the same or a different keyword chip.
    if (popup_view_->controller()->edit_model()->is_keyword_hint()) {
      const auto entry_method =
          event.IsMouseEvent() ? metrics::OmniboxEventProto::CLICK_HINT_VIEW
                               : metrics::OmniboxEventProto::TAP_HINT_VIEW;
      popup_view_->controller()->edit_model()->AcceptKeyword(entry_method);
    }
  } else {
    if (omnibox_feature_configs::Toolbelt::Get()
            .select_toolbelt_before_opening &&
        match().IsToolbelt()) {
      popup_view_->controller()->edit_model()->SetPopupSelection(selection);
    }
    WindowOpenDisposition disposition =
        ui::DispositionFromEventFlags(event.flags());
    popup_view_->controller()->edit_model()->OpenSelection(
        selection, event.time_stamp(), disposition,
        /*via_keyboard=*/event.IsKeyEvent());
  }
}

BEGIN_METADATA(OmniboxSuggestionButtonRowView)
END_METADATA
