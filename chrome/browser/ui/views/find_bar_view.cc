// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_view.h"

#include <algorithm>

#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
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
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

namespace {

// The default number of average characters that the text box will be.
constexpr int kDefaultCharWidth = 30;

// The minimum allowable width in chars for the find_text_ view. This ensures
// the view can at least display the caret and some number of characters.
constexpr int kMinimumCharWidth = 1;

// The match count label is like a normal label, but can process events (which
// makes it easier to forward events to the text input --- see
// FindBarView::TargetForRect).
class MatchCountLabel : public views::Label {
 public:
  MatchCountLabel() {}
  ~MatchCountLabel() override {}

  // views::Label overrides:
  bool CanProcessEventsWithinSubtree() const override { return true; }

  gfx::Size CalculatePreferredSize() const override {
    // We need to return at least 1dip so that box layout adds padding on either
    // side (otherwise there will be a jump when our size changes between empty
    // and non-empty).
    gfx::Size size = views::Label::CalculatePreferredSize();
    size.set_width(std::max(1, size.width()));
    return size;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MatchCountLabel);
};

// We use a hidden view to grab mouse clicks and bring focus to the find
// text box. This is because although the find text box may look like it
// extends all the way to the find button, it only goes as far as to the
// match_count label. The user, however, expects being able to click anywhere
// inside what looks like the find text box (including on or around the
// match_count label) and have focus brought to the find box.
class FocusForwarderView : public views::View {
 public:
  explicit FocusForwarderView(
      views::Textfield* view_to_focus_on_mousedown)
    : view_to_focus_on_mousedown_(view_to_focus_on_mousedown) {}

 private:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (view_to_focus_on_mousedown_)
      view_to_focus_on_mousedown_->RequestFocus();
    return true;
  }

  views::Textfield* view_to_focus_on_mousedown_;

  DISALLOW_COPY_AND_ASSIGN(FocusForwarderView);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FindBarView, public:

FindBarView::FindBarView(FindBarHost* host)
    : find_bar_host_(host),
      find_text_(new views::Textfield),
      match_count_text_(new MatchCountLabel()),
      focus_forwarder_view_(new FocusForwarderView(find_text_)),
      separator_(new views::Separator()),
      find_previous_button_(views::CreateVectorImageButton(this)),
      find_next_button_(views::CreateVectorImageButton(this)),
      close_button_(views::CreateVectorImageButton(this)) {
  find_text_->set_id(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);
  find_text_->SetDefaultWidthInChars(kDefaultCharWidth);
  find_text_->SetMinimumWidthInChars(kMinimumCharWidth);
  find_text_->set_controller(this);
  find_text_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FIND));
  find_text_->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  AddChildView(find_text_);

  find_previous_button_->set_id(VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON);
  find_previous_button_->SetFocusForPlatform();
  find_previous_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP));
  find_previous_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_PREVIOUS));
  AddChildView(find_previous_button_);

  find_next_button_->set_id(VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON);
  find_next_button_->SetFocusForPlatform();
  find_next_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_NEXT_TOOLTIP));
  find_next_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEXT));
  AddChildView(find_next_button_);

  close_button_->set_id(VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON);
  close_button_->SetFocusForPlatform();
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button_->SetAnimationDuration(0);
  AddChildView(close_button_);

  AddChildView(focus_forwarder_view_);

  EnableCanvasFlippingForRTLUI(true);

  match_count_text_->SetEventTargeter(
      std::make_unique<views::ViewTargeter>(this));
  AddChildViewAt(match_count_text_, 1);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  AddChildViewAt(separator_, 2);

  // Normally we could space objects horizontally by simply passing a constant
  // value to BoxLayout for between-child spacing.  But for the vector image
  // buttons, we want the spacing to apply between the inner "glyph" portions
  // of the buttons, ignoring the surrounding borders.  BoxLayout has no way
  // to dynamically adjust for this, so instead of using between-child spacing,
  // we place views directly adjacent, with horizontal margins on each view
  // that will add up to the right spacing amounts.

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
      views::kMarginsKey, new gfx::Insets(toast_control_vertical_margin +
                                          vector_button_horizontal_margin));
  find_next_button_->SetProperty(
      views::kMarginsKey, new gfx::Insets(toast_control_vertical_margin +
                                          vector_button_horizontal_margin));
  close_button_->SetProperty(views::kMarginsKey,
                             new gfx::Insets(toast_control_vertical_margin +
                                             vector_button_horizontal_margin));
  separator_->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(toast_control_vertical_margin + horizontal_margin));
  find_text_->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(toast_control_vertical_margin + horizontal_margin));
  match_count_text_->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(toast_label_vertical_margin + horizontal_margin));

  find_text_->SetBorder(views::NullBorder());

  auto* manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
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
  find_text_->SelectRange(selected_range);
}

base::string16 FindBarView::GetFindText() const {
  return find_text_->text();
}

gfx::Range FindBarView::GetSelectedRange() const {
  return find_text_->GetSelectedRange();
}

base::string16 FindBarView::GetFindSelectedText() const {
  return find_text_->GetSelectedText();
}

base::string16 FindBarView::GetMatchCountText() const {
  return match_count_text_->text();
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
  if (find_text_->text() != find_text && !find_text_->IsIMEComposing() &&
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

  match_count_text_->SetText(l10n_util::GetStringFUTF16(
      IDS_FIND_IN_PAGE_COUNT, base::FormatNumber(result.active_match_ordinal()),
      base::FormatNumber(result.number_of_matches())));

  UpdateMatchCountAppearance(result.number_of_matches() == 0 &&
                             result.final_update());

  // The match_count label may have increased/decreased in size so we need to
  // do a layout and repaint the dialog so that the find text field doesn't
  // partially overlap the match-count label when it increases on no matches.
  Layout();
  SchedulePaint();
}

void FindBarView::ClearMatchCount() {
  match_count_text_->SetText(base::string16());
  UpdateMatchCountAppearance(false);
  Layout();
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// FindBarView, views::View overrides:

void FindBarView::Layout() {
  views::View::Layout();

  // The focus forwarder view is a hidden view that should cover the area
  // between the find text box and the find button so that when the user clicks
  // in that area we focus on the find text box.
  const int find_text_edge = find_text_->x() + find_text_->width();
  focus_forwarder_view_->SetBounds(
      find_text_edge, find_previous_button_->y(),
      find_previous_button_->x() - find_text_edge,
      find_previous_button_->height());
}

gfx::Size FindBarView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Ignore the preferred size for the match count label, and just let it take
  // up part of the space for the input textfield. This prevents the overall
  // width from changing every time the match count text changes.
  size.set_width(size.width() - match_count_text_->GetPreferredSize().width());
  return size;
}

void FindBarView::AddedToWidget() {
  // Since the find bar now works/looks like a location bar bubble, make sure it
  // doesn't get dark themed in incognito mode.
  if (find_bar_host_->browser_view()->browser()->profile()->GetProfileType() ==
      Profile::INCOGNITO_PROFILE)
    SetNativeTheme(ui::NativeTheme::GetInstanceForNativeUi());
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, DropdownBarHostDelegate implementation:

void FindBarView::SetFocusAndSelection(bool select_all) {
  find_text_->RequestFocus();
#if !defined(OS_WIN)
  GetWidget()->GetInputMethod()->ShowVirtualKeyboardIfEnabled();
#endif
  if (select_all && !find_text_->text().empty())
    find_text_->SelectAll(true);
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::ButtonListener implementation:

void FindBarView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  switch (sender->id()) {
    case VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON:
    case VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON:
      if (!find_text_->text().empty()) {
        FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(
            find_bar_host_->GetFindBarController()->web_contents());
        find_tab_helper->StartFinding(
            find_text_->text(),
            sender->id() == VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON,
            false);  // Not case sensitive.
      }
      break;
    case VIEW_ID_FIND_IN_PAGE_CLOSE_BUTTON:
      find_bar_host_->GetFindBarController()->EndFindSession(
          FindBarController::kKeepSelectionOnPage,
          FindBarController::kKeepResultsInFindBox);
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
    base::string16 find_string = find_text_->text();
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
  if (!sender->IsIMEComposing() && sender->text() != last_searched_text_)
    Find(sender->text());
}

void FindBarView::OnAfterPaste() {
  // Clear the last search text so we always search for the user input after
  // a paste operation, even if the pasted text is the same as before.
  // See http://crbug.com/79002
  last_searched_text_.clear();
}

views::View* FindBarView::TargetForRect(View* root, const gfx::Rect& rect) {
  DCHECK_EQ(match_count_text_, root);
  return find_text_;
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
    find_tab_helper->StopFinding(FindBarController::kClearSelectionOnPage);
    UpdateForResult(find_tab_helper->find_result(), base::string16());
    find_bar_host_->MoveWindowIfNecessary(gfx::Rect());

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
  bool enable_buttons = !find_text_->text().empty() && !no_match;
  find_previous_button_->SetEnabled(enable_buttons);
  find_next_button_->SetEnabled(enable_buttons);
}

const char* FindBarView::GetClassName() const {
  return "FindBarView";
}

void FindBarView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  SkColor bg_color = theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultBackground);
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW, bg_color);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  const SkColor base_foreground_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);

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
