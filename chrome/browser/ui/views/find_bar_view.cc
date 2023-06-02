// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_view.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace {
void SetCommonButtonAttributes(views::ImageButton* button) {
  views::ConfigureVectorImageButton(button);
  views::InstallCircleHighlightPathGenerator(button);
}
}  // namespace

class FindBarMatchCountLabel : public views::Label {
 public:
  METADATA_HEADER(FindBarMatchCountLabel);

  FindBarMatchCountLabel() = default;

  FindBarMatchCountLabel(const FindBarMatchCountLabel&) = delete;
  FindBarMatchCountLabel& operator=(const FindBarMatchCountLabel&) = delete;

  ~FindBarMatchCountLabel() override = default;

  gfx::Size CalculatePreferredSize() const override {
    // We need to return at least 1dip so that box layout adds padding on either
    // side (otherwise there will be a jump when our size changes between empty
    // and non-empty).
    gfx::Size size = views::Label::CalculatePreferredSize();
    size.set_width(std::max(1, size.width()));
    return size;
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStatus;
    if (!last_result_) {
      node_data->SetNameExplicitlyEmpty();
    } else if (last_result_->number_of_matches() < 1) {
      node_data->SetNameChecked(
          l10n_util::GetStringUTF16(IDS_ACCESSIBLE_FIND_IN_PAGE_NO_RESULTS));
    } else {
      node_data->SetNameChecked(l10n_util::GetStringFUTF16(
          IDS_ACCESSIBLE_FIND_IN_PAGE_COUNT,
          base::FormatNumber(last_result_->active_match_ordinal()),
          base::FormatNumber(last_result_->number_of_matches())));
    }
  }

  void SetResult(const find_in_page::FindNotificationDetails& result) {
    if (last_result_ && result == *last_result_)
      return;

    last_result_ = result;
    SetText(l10n_util::GetStringFUTF16(
        IDS_FIND_IN_PAGE_COUNT,
        base::FormatNumber(last_result_->active_match_ordinal()),
        base::FormatNumber(last_result_->number_of_matches())));

    if (last_result_->final_update()) {
      NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged,
                               /* send_native_event = */ true);
    }
  }

  void ClearResult() {
    last_result_.reset();
    SetText(std::u16string());
  }

 private:
  absl::optional<find_in_page::FindNotificationDetails> last_result_;
};

BEGIN_VIEW_BUILDER(/* No Export */, FindBarMatchCountLabel, views::Label)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* No Export */, FindBarMatchCountLabel)

BEGIN_METADATA(FindBarMatchCountLabel, views::Label)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// FindBarView, public:

FindBarView::FindBarView(FindBarHost* host) {
  // Normally we could space objects horizontally by simply passing a constant
  // value to BoxLayout for between-child spacing.  But for the vector image
  // buttons, we want the spacing to apply between the inner "glyph" portions
  // of the buttons, ignoring the surrounding borders.  BoxLayout has no way
  // to dynamically adjust for this, so instead of using between-child spacing,
  // we place views directly adjacent, with horizontal margins on each view
  // that will add up to the right spacing amounts.

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const auto horizontal_margin = gfx::Insets::VH(
      0,
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL) / 2);
  const gfx::Insets vector_button =
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON);
  const auto vector_button_horizontal_margin =
      gfx::Insets::TLBR(0, horizontal_margin.left() - vector_button.left(), 0,
                        horizontal_margin.right() - vector_button.right());
  const auto toast_control_vertical_margin = gfx::Insets::VH(
      provider->GetDistanceMetric(DISTANCE_TOAST_CONTROL_VERTICAL), 0);
  const auto toast_label_vertical_margin = gfx::Insets::VH(
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL), 0);
  const auto image_button_margins =
      toast_control_vertical_margin + vector_button_horizontal_margin;

  // Align separator with textbox.
  const auto chrome_refresh_separator_vertical_margin =
      gfx::Insets::VH(views::LayoutProvider::Get()->GetDistanceMetric(
                          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                      0);
  views::Builder<FindBarView>(this)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetInsideBorderInsets(gfx::Insets(
          provider->GetInsetsMetric(INSETS_TOAST) - horizontal_margin))
      .SetHost(host)
      .SetFlipCanvasOnPaintForRTLUI(true)
      .AddChildren(
          views::Builder<views::Textfield>()
              .CopyAddressTo(&find_text_)
              .SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FIND))
              .SetBorder(views::NullBorder())
              .SetDefaultWidthInChars(30)
              .SetID(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD)
              .SetMinimumWidthInChars(1)
              .SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets(toast_control_vertical_margin +
                                       horizontal_margin))
              .SetController(this),
          views::Builder<FindBarMatchCountLabel>()
              .CopyAddressTo(&match_count_text_)
              .SetCanProcessEventsWithinSubtree(false)
              .SetProperty(
                  views::kMarginsKey,
                  gfx::Insets(toast_label_vertical_margin + horizontal_margin)),
          views::Builder<views::Separator>()
              .CopyAddressTo(&separator_)
              .SetCanProcessEventsWithinSubtree(false)
              .SetColorId(ui::kColorSeparator)
              .SetProperty(
                  views::kMarginsKey,
                  gfx::Insets(horizontal_margin +
                              (features::IsChromeRefresh2023()
                                   ? chrome_refresh_separator_vertical_margin
                                   : toast_control_vertical_margin))),
          views::Builder<views::ImageButton>()
              .CopyAddressTo(&find_previous_button_)
              .SetAccessibleName(
                  l10n_util::GetStringUTF16(IDS_ACCNAME_PREVIOUS))
              .SetID(VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON)
              .SetTooltipText(
                  l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP))
              .SetCallback(base::BindRepeating(&FindBarView::FindNext,
                                               base::Unretained(this), true))
              .SetProperty(views::kMarginsKey, image_button_margins),
          views::Builder<views::ImageButton>()
              .CopyAddressTo(&find_next_button_)
              .SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_NEXT))
              .SetID(VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON)
              .SetTooltipText(
                  l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_NEXT_TOOLTIP))
              .SetCallback(base::BindRepeating(&FindBarView::FindNext,
                                               base::Unretained(this), false))
              .SetProperty(views::kMarginsKey, image_button_margins),
          views::Builder<views::ImageButton>()
              .CopyAddressTo(&close_button_)
              .SetID(VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON)
              .SetTooltipText(
                  l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP))
              .SetAnimationDuration(base::TimeDelta())
              .SetCallback(base::BindRepeating(&FindBarView::EndFindSession,
                                               base::Unretained(this)))
              .SetProperty(views::kMarginsKey, image_button_margins))
      .BuildChildren();

  SetFlexForView(find_text_, 1, true);
  SetCommonButtonAttributes(find_previous_button_);
  SetCommonButtonAttributes(find_next_button_);
  SetCommonButtonAttributes(close_button_);
}

FindBarView::~FindBarView() {
}

void FindBarView::SetHost(FindBarHost* host) {
  find_bar_host_ = host;
  find_text_->SetShouldDoLearning(
      host && !host->browser_view()->GetProfile()->IsOffTheRecord());
}

void FindBarView::SetFindTextAndSelectedRange(
    const std::u16string& find_text,
    const gfx::Range& selected_range) {
  find_text_->SetText(find_text);
  find_text_->SetSelectedRange(selected_range);
  last_searched_text_ = find_text;
}

std::u16string FindBarView::GetFindText() const {
  return find_text_->GetText();
}

gfx::Range FindBarView::GetSelectedRange() const {
  return find_text_->GetSelectedRange();
}

std::u16string FindBarView::GetFindSelectedText() const {
  return find_text_->GetSelectedText();
}

std::u16string FindBarView::GetMatchCountText() const {
  return match_count_text_->GetText();
}

void FindBarView::UpdateForResult(
    const find_in_page::FindNotificationDetails& result,
    const std::u16string& find_text) {
  bool have_valid_range =
      result.number_of_matches() != -1 && result.active_match_ordinal() != -1;

  // http://crbug.com/34970: some IMEs get confused if we change the text
  // composed by them. To avoid this problem, we should check the IME status and
  // update the text only when the IME is not composing text.
  //
  // Find Bar hosts with global find pasteboards are expected to preserve the
  // find text contents after clearing the find results as the normal
  // prepopulation code does not run.
  if (find_text_->GetText() != find_text && !find_text_->IsIMEComposing() &&
      (!find_bar_host_ || !find_bar_host_->HasGlobalFindPasteboard() ||
       !find_text.empty())) {
    find_text_->SetText(find_text);
    find_text_->SelectAll(true);
  }

  if (find_text.empty() || !have_valid_range) {
    // If there was no text entered, we don't show anything in the result count
    // area.
    ClearMatchCount();
    return;
  }

  match_count_text_->SetResult(result);

  UpdateMatchCountAppearance(result.number_of_matches() == 0 &&
                             result.final_update());

  // The match_count label may have increased/decreased in size so we need to
  // do a layout and repaint the dialog so that the find text field doesn't
  // partially overlap the match-count label when it increases on no matches.
  Layout();
  SchedulePaint();
}

void FindBarView::ClearMatchCount() {
  match_count_text_->ClearResult();
  UpdateMatchCountAppearance(false);
  Layout();
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// FindBarView, views::View overrides:

bool FindBarView::OnMousePressed(const ui::MouseEvent& event) {
  // The find text box only extends to the match count label.  However, users
  // expect to be able to click anywhere inside what looks like the find text
  // box (including on or around the match_count label) and have focus brought
  // to the find box.  Cause clicks between the textfield and the find previous
  // button to focus the textfield.
  const int find_text_edge = find_text_->bounds().right();
  const gfx::Rect focus_area(find_text_edge, find_previous_button_->y(),
                             find_previous_button_->x() - find_text_edge,
                             find_previous_button_->height());
  if (!GetMirroredRect(focus_area).Contains(event.location()))
    return false;
  find_text_->RequestFocus();
  return true;
}

gfx::Size FindBarView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Ignore the preferred size for the match count label, and just let it take
  // up part of the space for the input textfield. This prevents the overall
  // width from changing every time the match count text changes.
  size.set_width(size.width() - match_count_text_->GetPreferredSize().width());
  return size;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, DropdownBarHostDelegate implementation:

void FindBarView::FocusAndSelectAll() {
  find_text_->RequestFocus();
#if !BUILDFLAG(IS_WIN)
  GetWidget()->GetInputMethod()->SetVirtualKeyboardVisibilityIfEnabled(true);
#endif
  if (!find_text_->GetText().empty())
    find_text_->SelectAll(true);
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::TextfieldController implementation:

bool FindBarView::HandleKeyEvent(views::Textfield* sender,
                                 const ui::KeyEvent& key_event) {
  // If the dialog is not visible, there is no reason to process keyboard input.
  if (!find_bar_host_ || !find_bar_host_->IsVisible())
    return false;

  if (find_bar_host_->MaybeForwardKeyEventToWebpage(key_event))
    return true;  // Handled, we are done!

  if (key_event.key_code() == ui::VKEY_RETURN &&
      key_event.type() == ui::ET_KEY_PRESSED) {
    // Pressing Return/Enter starts the search (unless text box is empty).
    std::u16string find_string = find_text_->GetText();
    if (!find_string.empty()) {
      FindBarController* controller = find_bar_host_->GetFindBarController();
      find_in_page::FindTabHelper* find_tab_helper =
          find_in_page::FindTabHelper::FromWebContents(
              controller->web_contents());
      // Search forwards for enter, backwards for shift-enter.
      find_tab_helper->StartFinding(
          find_string, !key_event.IsShiftDown() /* forward_direction */,
          false /* case_sensitive */,
          true /* find_match */);
    }
    return true;
  }

  return false;
}

void FindBarView::OnAfterUserAction(views::Textfield* sender) {
  // The composition text wouldn't be what the user is really looking for.
  // We delay the search until the user commits the composition text.
  if (!sender->IsIMEComposing() && sender->GetText() != last_searched_text_)
    Find(sender->GetText());
}

void FindBarView::OnAfterPaste() {
  // Clear the last search text so we always search for the user input after
  // a paste operation, even if the pasted text is the same as before.
  // See http://crbug.com/79002
  last_searched_text_.clear();
}

void FindBarView::Find(const std::u16string& search_text) {
  DCHECK(find_bar_host_);
  FindBarController* controller = find_bar_host_->GetFindBarController();
  DCHECK(controller);
  content::WebContents* web_contents = controller->web_contents();
  // We must guard against a NULL web_contents, which can happen if the text
  // in the Find box is changed right after the tab is destroyed. Otherwise, it
  // can lead to crashes, as exposed by automation testing in issue 8048.
  if (!web_contents)
    return;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);

  last_searched_text_ = search_text;

  controller->OnUserChangedFindText(search_text);

  // Initiate a search (even though old searches might be in progress).
  find_tab_helper->StartFinding(search_text, true /* forward_direction */,
                                false /* case_sensitive */,
                                true /* find_match */);
}

void FindBarView::FindNext(bool reverse) {
  if (!find_bar_host_)
    return;
  if (!find_text_->GetText().empty()) {
    find_in_page::FindTabHelper* find_tab_helper =
        find_in_page::FindTabHelper::FromWebContents(
            find_bar_host_->GetFindBarController()->web_contents());
    find_tab_helper->StartFinding(find_text_->GetText(),
                                  !reverse, /* forward_direction */
                                  false,    /* case_sensitive */
                                  true /* find_match */);
  }
}

void FindBarView::EndFindSession() {
  if (!find_bar_host_)
    return;
  find_bar_host_->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
}

void FindBarView::UpdateMatchCountAppearance(bool no_match) {
  bool enable_buttons = !find_text_->GetText().empty() && !no_match;
  find_previous_button_->SetEnabled(enable_buttons);
  find_next_button_->SetEnabled(enable_buttons);
}

void FindBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      kColorFindBarBackground);

  border->SetCornerRadius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kFindBarViewRadius));

  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  const ui::ColorProvider* color_provider = GetColorProvider();
  match_count_text_->SetBackgroundColor(
      color_provider->GetColor(kColorFindBarBackground));
  match_count_text_->SetEnabledColor(
      color_provider->GetColor(kColorFindBarMatchCount));

  const SkColor fg_color = color_provider->GetColor(kColorFindBarButtonIcon);
  const SkColor fg_disabled_color =
      color_provider->GetColor(kColorFindBarButtonIconDisabled);
  views::SetImageFromVectorIconWithColor(find_previous_button_,
                                         features::IsChromeRefresh2023()
                                             ? kKeyboardArrowUpChromeRefreshIcon
                                             : vector_icons::kCaretUpIcon,
                                         fg_color, fg_disabled_color);
  views::SetImageFromVectorIconWithColor(
      find_next_button_,
      features::IsChromeRefresh2023() ? kKeyboardArrowDownChromeRefreshIcon
                                      : vector_icons::kCaretDownIcon,
      fg_color, fg_disabled_color);
  views::SetImageFromVectorIconWithColor(close_button_,
                                         features::IsChromeRefresh2023()
                                             ? kCloseChromeRefreshIcon
                                             : vector_icons::kCloseRoundedIcon,
                                         fg_color, fg_disabled_color);
}

BEGIN_METADATA(FindBarView, views::View)
END_METADATA
