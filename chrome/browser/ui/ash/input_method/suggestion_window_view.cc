// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/suggestion_window_view.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "chrome/browser/ui/ash/input_method/border_factory.h"
#include "chrome/browser/ui/ash/input_method/colors.h"
#include "chrome/browser/ui/ash/input_method/completion_suggestion_view.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_properties.h"

namespace ui {
namespace ime {

namespace {

bool ShouldHighlight(const views::Button& button) {
  return button.GetState() == views::Button::STATE_HOVERED ||
         button.GetState() == views::Button::STATE_PRESSED;
}

// TODO(b/1101669): Create abstract HighlightableButton for learn_more button,
// setting_link_, suggestion_view and undo_view.
void SetHighlighted(views::View& view, bool highlighted) {
  if (!!view.background() != highlighted) {
    view.SetBackground(highlighted
                           ? views::CreateRoundedRectBackground(
                                 ResolveSemanticColor(kButtonHighlightColor), 2)
                           : nullptr);
  }
}

}  // namespace

// static
SuggestionWindowView* SuggestionWindowView::Create(gfx::NativeView parent,
                                                   AssistiveDelegate* delegate,
                                                   Orientation orientation) {
  auto* const view = new SuggestionWindowView(parent, delegate, orientation);
  views::Widget* const widget =
      views::BubbleDialogDelegateView::CreateBubble(view);
  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);
  return view;
}

std::unique_ptr<views::NonClientFrameView>
SuggestionWindowView::CreateNonClientFrameView(views::Widget* widget) {
  std::unique_ptr<views::NonClientFrameView> frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(GetBorderForWindow(WindowBorderType::Suggestion));
  return frame;
}

void SuggestionWindowView::Show(const SuggestionDetails& details) {
  ResizeCandidateArea({});
  Reorient(Orientation::kVertical);
  completion_view_->SetVisible(true);
  completion_view_->SetView(details);
  if (details.show_setting_link) {
    completion_view_->SetMinWidth(
        setting_link_
            ->GetPreferredSize(views::SizeBounds(setting_link_->width(), {}))
            .width());
  }

  setting_link_->SetVisible(details.show_setting_link);

  MakeVisible();
}

void SuggestionWindowView::ShowMultipleCandidates(
    const ash::input_method::AssistiveWindowProperties& properties,
    Orientation orientation) {
  const std::vector<std::u16string>& candidates = properties.candidates;
  completion_view_->SetVisible(false);
  Reorient(orientation, /*extra_padding_on_right=*/
           properties.type !=
               ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion);
  ResizeCandidateArea(
      candidates,
      properties.type == ash::ime::AssistiveWindowType::kEmojiSuggestion);
  learn_more_button_->SetVisible(properties.show_setting_link);
  type_ = properties.type;
  // Ensure colours are correct.
  OnThemeChanged();
  MakeVisible();
}

void SuggestionWindowView::SetButtonHighlighted(
    const AssistiveWindowButton& button,
    bool highlighted) {
  if (button.id == ButtonId::kSuggestion) {
    if (completion_view_->GetVisible()) {
      completion_view_->SetHighlighted(highlighted);
    } else {
      const views::View::Views& candidate_buttons =
          multiple_candidate_area_->children();
      if (button.suggestion_index < candidate_buttons.size()) {
        SetCandidateHighlighted(static_cast<IndexedSuggestionCandidateButton*>(
                                    candidate_buttons[button.suggestion_index]),
                                highlighted);
      }
    }
  } else if (button.id == ButtonId::kSmartInputsSettingLink) {
    SetHighlighted(*setting_link_, highlighted);
  } else if (button.id == ButtonId::kLearnMore) {
    SetHighlighted(*learn_more_button_, highlighted);
  }
}

gfx::Rect SuggestionWindowView::GetBubbleBounds() {
  // The bubble bounds must be shifted to align with the anchor if there is a
  // completion view.
  const gfx::Point anchor_origin = completion_view_->GetVisible()
                                       ? completion_view_->GetAnchorOrigin()
                                       : gfx::Point(0, 0);
  return BubbleDialogDelegateView::GetBubbleBounds() -
         anchor_origin.OffsetFromOrigin();
}

void SuggestionWindowView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  if (type_ == ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion) {
    const int inset = views::LayoutProvider::Get()->GetDistanceMetric(
        views::DistanceMetric::DISTANCE_VECTOR_ICON_PADDING);
    learn_more_button_->SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(
            gfx::Insets::TLBR(inset, 0, inset, inset),
            color_provider->GetColor(ui::kColorButtonBackground)),
        views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));
  } else {
    learn_more_button_->SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(
            gfx::Insets::TLBR(
                views::LayoutProvider::Get()->GetShadowElevationMetric(
                    views::Emphasis::kLow),
                0, 0, 0),
            color_provider->GetColor(ui::kColorBubbleFooterBorder)),
        views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));
  }
  // TODO(crbug.com/1099044): Update and use cros colors.
  learn_more_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsOutlineIcon,
                                     ui::kColorIconSecondary));
}

SuggestionWindowView::SuggestionWindowView(gfx::NativeView parent,
                                           AssistiveDelegate* delegate,
                                           Orientation orientation)
    : delegate_(delegate) {
  DCHECK(parent);
  // AccessibleRole determines whether the content is announced on pop-up.
  // Inner content should not be announced when the window appears since this
  // is handled by AssistiveAccessibilityView to announce a custom string.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  set_parent_window(parent);
  set_margins(gfx::Insets());
  set_adjust_if_offscreen(true);

  completion_view_ = AddChildView(
      std::make_unique<CompletionSuggestionView>(base::BindRepeating(
          &AssistiveDelegate::AssistiveWindowButtonClicked,
          base::Unretained(delegate_),
          AssistiveWindowButton{.id = ui::ime::ButtonId::kSuggestion,
                                .suggestion_index = 0})));
  completion_view_->SetVisible(false);
  multiple_candidate_area_ = AddChildView(std::make_unique<views::View>());
  multiple_candidate_area_->SetVisible(false);
  Reorient(orientation);

  setting_link_ = AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_SUGGESTION_LEARN_MORE)));
  setting_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // TODO(crbug.com/40138695): Implement proper UI layout using Insets constant.
  constexpr auto insets = gfx::Insets::TLBR(0, kPadding, kPadding, kPadding);
  setting_link_->SetBorder(views::CreateEmptyBorder(insets));
  constexpr int kSettingLinkFontSize = 11;
  setting_link_->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::ITALIC,
                                           kSettingLinkFontSize,
                                           gfx::Font::Weight::NORMAL));
  const auto on_setting_link_clicked = [](AssistiveDelegate* delegate) {
    delegate->AssistiveWindowButtonClicked(
        {.id = ButtonId::kSmartInputsSettingLink});
  };
  setting_link_->SetCallback(
      base::BindRepeating(on_setting_link_clicked, delegate_));
  setting_link_->SetVisible(false);

  learn_more_button_ =
      AddChildView(std::make_unique<views::ImageButton>(base::BindRepeating(
          &SuggestionWindowView::LearnMoreClicked, base::Unretained(this))));
  learn_more_button_->SetImageHorizontalAlignment(
      views::ImageButton::ALIGN_CENTER);
  learn_more_button_->SetImageVerticalAlignment(
      views::ImageButton::ALIGN_MIDDLE);
  learn_more_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  const auto update_button_highlight = [](views::Button* button) {
    SetHighlighted(*button, ShouldHighlight(*button));
  };
  auto subscription =
      learn_more_button_->AddStateChangedCallback(base::BindRepeating(
          update_button_highlight, base::Unretained(learn_more_button_)));
  subscriptions_.insert({learn_more_button_, std::move(subscription)});
  learn_more_button_->SetVisible(false);
}

SuggestionWindowView::~SuggestionWindowView() = default;

void SuggestionWindowView::LearnMoreClicked() {
  delegate_->AssistiveWindowButtonClicked(AssistiveWindowButton{
      .id = ui::ime::ButtonId::kLearnMore, .window_type = type_});
}

raw_ptr<views::ImageButton> SuggestionWindowView::getLearnMoreButton() {
  return learn_more_button_;
}

void SuggestionWindowView::ResizeCandidateArea(
    const std::vector<std::u16string>& new_candidates,
    bool use_legacy_candidate) {
  const views::View::Views& candidates = multiple_candidate_area_->children();
  while (candidates.size()) {
    subscriptions_.erase(
        multiple_candidate_area_->RemoveChildViewT(candidates.back()).get());
  }

  for (size_t index = 0; index < new_candidates.size(); ++index) {
    auto* const candidate = multiple_candidate_area_->AddChildView(
        std::make_unique<IndexedSuggestionCandidateButton>(
            base::BindRepeating(
                &AssistiveDelegate::AssistiveWindowButtonClicked,
                base::Unretained(delegate_),
                AssistiveWindowButton{.id = ui::ime::ButtonId::kSuggestion,
                                      .suggestion_index = index}),
            /* candidate_text=*/new_candidates[index],
            // Label indexes start from "1", hence we increment index by one.
            /* index_text=*/base::FormatNumber(index + 1),
            use_legacy_candidate));
    // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
    candidate->SetLayoutManagerUseConstrainedSpace(false);

    auto subscription = candidate->AddStateChangedCallback(base::BindRepeating(
        [](SuggestionWindowView* window,
           IndexedSuggestionCandidateButton* button) {
          window->SetCandidateHighlighted(button, ShouldHighlight(*button));
        },
        base::Unretained(this), base::Unretained(candidate)));
    subscriptions_.insert({candidate, std::move(subscription)});
  }
}

void SuggestionWindowView::Reorient(Orientation orientation,
                                    bool extra_padding_on_right) {
  views::BoxLayout::Orientation layout_orientation =
      views::BoxLayout::Orientation::kVertical;
  int multiple_candidate_area_padding = 0;
  switch (orientation) {
    case Orientation::kVertical: {
      layout_orientation = views::BoxLayout::Orientation::kVertical;
      // TODO(jhtin): remove this when emoji uses horizontal layout.
      multiple_candidate_area_padding = 0;
      break;
    }
    case Orientation::kHorizontal: {
      layout_orientation = views::BoxLayout::Orientation::kHorizontal;
      multiple_candidate_area_padding = 4;
      break;
    }
  }

  SetLayoutManager(std::make_unique<views::BoxLayout>(layout_orientation));
  gfx::Insets inset(multiple_candidate_area_padding);
  if (!extra_padding_on_right) {
    inset.set_right(0);
  }
  multiple_candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      layout_orientation, inset,
      /* between_child_spacing=*/multiple_candidate_area_padding));
}

void SuggestionWindowView::MakeVisible() {
  multiple_candidate_area_->SetVisible(true);
  SizeToContents();
  // Docs can put the cursor offscreen - force it onscreen.
  GetWidget()->SetBoundsConstrained(GetBubbleBounds());
}

void SuggestionWindowView::SetCandidateHighlighted(
    IndexedSuggestionCandidateButton* view,
    bool highlighted) {
  // Clear all highlights if any exists.
  for (views::View* candidate_button : multiple_candidate_area_->children()) {
    static_cast<IndexedSuggestionCandidateButton*>(candidate_button)
        ->SetHighlight(false);
  }

  if (highlighted) {
    view->SetHighlight(highlighted);
  }
}

BEGIN_METADATA(SuggestionWindowView)
END_METADATA

}  // namespace ime
}  // namespace ui
