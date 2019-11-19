// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_view.h"

#include <algorithm>
#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
void SetCommonButtonAttributes(views::ImageButton* button) {
  views::ConfigureVectorImageButton(button);
  views::InstallCircleHighlightPathGenerator(button);
  button->SetFocusForPlatform();
}
}  // namespace

class FindBarView::MatchCountLabel : public views::Label {
 public:
  MatchCountLabel() {}
  ~MatchCountLabel() override {}

  gfx::Size CalculatePreferredSize() const override {
    // We need to return at least 1dip so that box layout adds padding on either
    // side (otherwise there will be a jump when our size changes between empty
    // and non-empty).
    gfx::Size size = views::Label::CalculatePreferredSize();
    size.set_width(std::max(1, size.width()));
    return size;
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    if (!last_result_) {
      node_data->SetNameExplicitlyEmpty();
    } else if (last_result_->number_of_matches() < 1) {
      node_data->SetName(
          l10n_util::GetStringUTF16(IDS_ACCESSIBLE_FIND_IN_PAGE_NO_RESULTS));
    } else {
      node_data->SetName(l10n_util::GetStringFUTF16(
          IDS_ACCESSIBLE_FIND_IN_PAGE_COUNT,
          base::FormatNumber(last_result_->active_match_ordinal()),
          base::FormatNumber(last_result_->number_of_matches())));
    }
    node_data->role = ax::mojom::Role::kStatus;
  }

  void SetResult(const FindNotificationDetails& result) {
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
    SetText(base::string16());
  }

 private:
  base::Optional<FindNotificationDetails> last_result_;

  DISALLOW_COPY_AND_ASSIGN(MatchCountLabel);
};

////////////////////////////////////////////////////////////////////////////////
// FindBarView, public:

FindBarView::FindBarView(FindBarHost* host) : find_bar_host_(host) {
  auto find_text = std::make_unique<views::Textfield>();
  find_text->SetID(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);
  find_text->SetDefaultWidthInChars(30);
  find_text->SetMinimumWidthInChars(1);
  find_text->set_controller(this);
  find_text->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FIND));
  find_text->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  find_text_ = AddChildView(std::move(find_text));

  auto match_count_text = std::make_unique<MatchCountLabel>();
  match_count_text->set_can_process_events_within_subtree(false);
  match_count_text_ = AddChildView(std::move(match_count_text));

  auto separator = std::make_unique<views::Separator>();
  separator->set_can_process_events_within_subtree(false);
  separator_ = AddChildView(std::move(separator));

  auto find_previous_button = std::make_unique<views::ImageButton>(this);
  SetCommonButtonAttributes(find_previous_button.get());
  find_previous_button->SetID(VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON);
  find_previous_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP));
  find_previous_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_PREVIOUS));
  find_previous_button_ = AddChildView(std::move(find_previous_button));

  auto find_next_button = std::make_unique<views::ImageButton>(this);
  SetCommonButtonAttributes(find_next_button.get());
  find_next_button->SetID(VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON);
  find_next_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_NEXT_TOOLTIP));
  find_next_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEXT));
  find_next_button_ = AddChildView(std::move(find_next_button));

  auto close_button = std::make_unique<views::ImageButton>(this);
  SetCommonButtonAttributes(close_button.get());
  close_button->SetID(VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON);
  close_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP));
  close_button->SetAnimationDuration(base::TimeDelta());
  close_button_ = AddChildView(std::move(close_button));

  EnableCanvasFlippingForRTLUI(true);

  // Normally we could space objects horizontally by simply passing a constant
  // value to BoxLayout for between-child spacing.  But for the vector image
  // buttons, we want the spacing to apply between the inner "glyph" portions
  // of the buttons, ignoring the surrounding borders.  BoxLayout has no way
  // to dynamically adjust for this, so instead of using between-child spacing,
  // we place views directly adjacent, with horizontal margins on each view
  // that will add up to the right spacing amounts.

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets horizontal_margin(
      0,
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL) / 2);
  const gfx::Insets vector_button =
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON);
  const gfx::Insets vector_button_horizontal_margin(
      0, horizontal_margin.left() - vector_button.left(), 0,
      horizontal_margin.right() - vector_button.right());
  const gfx::Insets toast_control_vertical_margin(
      provider->GetDistanceMetric(DISTANCE_TOAST_CONTROL_VERTICAL), 0);
  const gfx::Insets toast_label_vertical_margin(
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL), 0);
  find_previous_button_->SetProperty(
      views::kMarginsKey, gfx::Insets(toast_control_vertical_margin +
                                      vector_button_horizontal_margin));
  find_next_button_->SetProperty(views::kMarginsKey,
                                 gfx::Insets(toast_control_vertical_margin +
                                             vector_button_horizontal_margin));
  close_button_->SetProperty(views::kMarginsKey,
                             gfx::Insets(toast_control_vertical_margin +
                                         vector_button_horizontal_margin));
  separator_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(toast_control_vertical_margin + horizontal_margin));
  find_text_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(toast_control_vertical_margin + horizontal_margin));
  match_count_text_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(toast_label_vertical_margin + horizontal_margin));

  find_text_->SetBorder(views::NullBorder());

  auto* manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(provider->GetInsetsMetric(INSETS_TOAST) - horizontal_margin),
      0));

  manager->SetFlexForView(find_text_, 1, true);
}

FindBarView::~FindBarView() {
}

void FindBarView::SetFindTextAndSelectedRange(
    const base::string16& find_text,
    const gfx::Range& selected_range) {
  find_text_->SetText(find_text);
  find_text_->SetSelectedRange(selected_range);
}

base::string16 FindBarView::GetFindText() const {
  return find_text_->GetText();
}

gfx::Range FindBarView::GetSelectedRange() const {
  return find_text_->GetSelectedRange();
}

base::string16 FindBarView::GetFindSelectedText() const {
  return find_text_->GetSelectedText();
}

base::string16 FindBarView::GetMatchCountText() const {
  return match_count_text_->GetText();
}

void FindBarView::UpdateForResult(const FindNotificationDetails& result,
                                  const base::string16& find_text) {
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
      (!find_bar_host_->HasGlobalFindPasteboard() || !find_text.empty())) {
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
#if !defined(OS_WIN)
  GetWidget()->GetInputMethod()->ShowVirtualKeyboardIfEnabled();
#endif
  if (!find_text_->GetText().empty())
    find_text_->SelectAll(true);
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::ButtonListener implementation:

void FindBarView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  switch (sender->GetID()) {
    case VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON:
    case VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON:
      if (!find_text_->GetText().empty()) {
        FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(
            find_bar_host_->GetFindBarController()->web_contents());
        find_tab_helper->StartFinding(
            find_text_->GetText(),
            sender->GetID() == VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON,
            false);  // Not case sensitive.
      }
      break;
    case VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON:
      find_bar_host_->GetFindBarController()->EndFindSession(
          FindOnPageSelectionAction::kKeep, FindBoxResultAction::kKeep);
      break;
    default:
      NOTREACHED() << "Unknown button";
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::TextfieldController implementation:

bool FindBarView::HandleKeyEvent(views::Textfield* sender,
                                 const ui::KeyEvent& key_event) {
  // If the dialog is not visible, there is no reason to process keyboard input.
  if (!find_bar_host_->IsVisible())
    return false;

  if (find_bar_host_->MaybeForwardKeyEventToWebpage(key_event))
    return true;  // Handled, we are done!

  if (key_event.key_code() == ui::VKEY_RETURN &&
      key_event.type() == ui::ET_KEY_PRESSED) {
    // Pressing Return/Enter starts the search (unless text box is empty).
    base::string16 find_string = find_text_->GetText();
    if (!find_string.empty()) {
      FindBarController* controller = find_bar_host_->GetFindBarController();
      FindTabHelper* find_tab_helper =
          FindTabHelper::FromWebContents(controller->web_contents());
      // Search forwards for enter, backwards for shift-enter.
      find_tab_helper->StartFinding(find_string,
                                    !key_event.IsShiftDown(),
                                    false);  // Not case sensitive.
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

void FindBarView::Find(const base::string16& search_text) {
  FindBarController* controller = find_bar_host_->GetFindBarController();
  DCHECK(controller);
  content::WebContents* web_contents = controller->web_contents();
  // We must guard against a NULL web_contents, which can happen if the text
  // in the Find box is changed right after the tab is destroyed. Otherwise, it
  // can lead to crashes, as exposed by automation testing in issue 8048.
  if (!web_contents)
    return;
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(web_contents);

  last_searched_text_ = search_text;

  controller->OnUserChangedFindText(search_text);

  // When the user changes something in the text box we check the contents and
  // if the textbox contains something we set it as the new search string and
  // initiate search (even though old searches might be in progress).
  if (!search_text.empty()) {
    // The last two params here are forward (true) and case sensitive (false).
    find_tab_helper->StartFinding(search_text, true, false);
  } else {
    find_tab_helper->StopFinding(FindOnPageSelectionAction::kClear);
    UpdateForResult(find_tab_helper->find_result(), base::string16());
    find_bar_host_->MoveWindowIfNecessary();

    // Clearing the text box should clear the prepopulate state so that when
    // we close and reopen the Find box it doesn't show the search we just
    // deleted. We can't do this on ChromeOS yet because we get ContentsChanged
    // sent for a lot more things than just the user nulling out the search
    // terms. See http://crbug.com/45372.
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    FindBarState* find_bar_state = FindBarStateFactory::GetForProfile(profile);
    find_bar_state->set_last_prepopulate_text(base::string16());
  }
}

void FindBarView::UpdateMatchCountAppearance(bool no_match) {
  bool enable_buttons = !find_text_->GetText().empty() && !no_match;
  find_previous_button_->SetEnabled(enable_buttons);
  find_next_button_->SetEnabled(enable_buttons);
}

const char* FindBarView::GetClassName() const {
  return "FindBarView";
}

void FindBarView::OnThemeChanged() {
  ui::NativeTheme* theme = GetNativeTheme();
  SkColor bg_color =
      SkColorSetA(theme->GetSystemColor(
                      ui::NativeTheme::kColorId_TextfieldDefaultBackground),
                  0xFF);
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW, bg_color);
  // TODO(tluk): Remove when fixing https://crbug.com/822075 and use
  // EMPHASIS_HIGH metric values from the LayoutProvider to get the
  // corner radius.
  border->SetCornerRadius(2);

  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  const SkColor base_foreground_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_TextfieldDefaultColor);

  match_count_text_->SetBackgroundColor(bg_color);
  match_count_text_->SetEnabledColor(
      SkColorSetA(base_foreground_color, gfx::kGoogleGreyAlpha700));
  separator_->SetColor(
      SkColorSetA(base_foreground_color, gfx::kGoogleGreyAlpha300));

  views::SetImageFromVectorIcon(find_previous_button_, kCaretUpIcon,
                                base_foreground_color);
  views::SetImageFromVectorIcon(find_next_button_, kCaretDownIcon,
                                base_foreground_color);
  views::SetImageFromVectorIcon(close_button_, vector_icons::kCloseRoundedIcon,
                                base_foreground_color);
}
