// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// TODO(djacobo): Replace this limit to correctly reflect the UI mocks, which
// now instead of limiting the results to 3.5 will allow whatever fits in 256pt.
// Using |kMaxAppResults| as a measure of how many apps we want to show.
constexpr size_t kMaxAppResults =
    chromeos::AppsNavigationThrottle::kMaxAppResults;
// Main components sizes
constexpr int kTitlePadding = 16;
constexpr int kRowHeight = 32;
constexpr int kMaxWidth = 320;
constexpr gfx::Insets kSeparatorPadding(16, 0, 16, 0);
constexpr SkColor kSeparatorColor = SkColorSetARGB(0x1F, 0x0, 0x0, 0x0);

// UI position wrt the Top Container
constexpr int kTopContainerMerge = 3;

constexpr char kInvalidLaunchName[] = "";

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
}

views::Separator* CreateHorizontalSeparator() {
  views::Separator* separator = new views::Separator;
  separator->SetColor(kSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(kSeparatorPadding));
  return separator;
}

}  // namespace

// IntentPickerLabelButton

// A button that represents a candidate intent handler.
class IntentPickerLabelButton : public views::LabelButton {
 public:
  IntentPickerLabelButton(views::ButtonListener* listener,
                          const gfx::Image* icon,
                          const std::string& launch_name,
                          const std::string& display_name)
      : LabelButton(listener,
                    base::UTF8ToUTF16(base::StringPiece(display_name))),
        launch_name_(launch_name) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetMinSize(gfx::Size(kMaxWidth, kRowHeight));
    SetInkDropMode(InkDropMode::ON);
    if (!icon->IsEmpty())
      SetImage(views::ImageButton::STATE_NORMAL, *icon->ToImageSkia());
    SetBorder(views::CreateEmptyBorder(8, 16, 8, 0));
    SetFocusForPlatform();
    set_ink_drop_base_color(SK_ColorGRAY);
    set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);
  }

  void MarkAsUnselected(const ui::Event* event) {
    AnimateInkDrop(views::InkDropState::HIDDEN,
                   ui::LocatedEvent::FromIfValid(event));
  }

  void MarkAsSelected(const ui::Event* event) {
    AnimateInkDrop(views::InkDropState::ACTIVATED,
                   ui::LocatedEvent::FromIfValid(event));
  }

  views::InkDropState GetTargetInkDropState() {
    return GetInkDrop()->GetTargetInkDropState();
  }

 private:
  std::string launch_name_;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerLabelButton);
};

// static
IntentPickerBubbleView* IntentPickerBubbleView::intent_picker_bubble_ = nullptr;

// static
views::Widget* IntentPickerBubbleView::ShowBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    std::vector<AppInfo> app_info,
    bool disable_stay_in_chrome,
    IntentPickerResponse intent_picker_cb) {
  if (intent_picker_bubble_) {
    views::Widget* widget =
        views::BubbleDialogDelegateView::CreateBubble(intent_picker_bubble_);
    widget->Show();
    return widget;
  }
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || !BrowserView::GetBrowserViewForBrowser(browser)) {
    std::move(intent_picker_cb)
        .Run(kInvalidLaunchName, apps::mojom::AppType::kUnknown,
             chromeos::IntentPickerCloseReason::ERROR, false);
    return nullptr;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  intent_picker_bubble_ = new IntentPickerBubbleView(
      std::move(app_info), std::move(intent_picker_cb), web_contents,
      disable_stay_in_chrome);
  intent_picker_bubble_->set_margins(gfx::Insets());

  if (anchor_view) {
    intent_picker_bubble_->SetAnchorView(anchor_view);
    intent_picker_bubble_->SetArrow(views::BubbleBorder::TOP_RIGHT);
  } else {
    intent_picker_bubble_->set_parent_window(browser_view->GetNativeWindow());
    // Using the TopContainerBoundsInScreen Rect to specify an anchor for the
    // the UI. Rect allow us to set the coordinates(x,y), the width and height
    // for the new Rectangle.
    intent_picker_bubble_->SetAnchorRect(
        gfx::Rect(browser_view->GetTopContainerBoundsInScreen().x(),
                  browser_view->GetTopContainerBoundsInScreen().y(),
                  browser_view->GetTopContainerBoundsInScreen().width(),
                  browser_view->GetTopContainerBoundsInScreen().height() -
                      kTopContainerMerge));
  }
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(intent_picker_bubble_);
  intent_picker_bubble_->GetDialogClientView()->Layout();
  // TODO(aleventhal) Should not need to be focusable as only descendant widgets
  // are interactive; however, it does call RequestFocus(). If it is going to be
  // focusable, it needs an accessible name so that it can pass accessibility
  // checks. Use the same accessible name as the icon. Set the role as kDialog
  // to ensure screen readers immediately announce the text of this view.
  intent_picker_bubble_->GetViewAccessibility().OverrideRole(
      ax::mojom::Role::kDialog);
  intent_picker_bubble_->GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON));
  intent_picker_bubble_->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  DCHECK(intent_picker_bubble_->HasCandidates());
  intent_picker_bubble_->GetIntentPickerLabelButtonAt(0)->MarkAsSelected(
      nullptr);
  widget->Show();
  return widget;
}

// static
std::unique_ptr<IntentPickerBubbleView>
IntentPickerBubbleView::CreateBubbleView(std::vector<AppInfo> app_info,
                                         bool disable_stay_in_chrome,
                                         IntentPickerResponse intent_picker_cb,
                                         content::WebContents* web_contents) {
  std::unique_ptr<IntentPickerBubbleView> bubble(new IntentPickerBubbleView(
      std::move(app_info), std::move(intent_picker_cb), web_contents,
      disable_stay_in_chrome));
  bubble->Init();
  return bubble;
}

// static
void IntentPickerBubbleView::CloseCurrentBubble() {
  if (intent_picker_bubble_)
    intent_picker_bubble_->CloseBubble();
}

void IntentPickerBubbleView::CloseBubble() {
  intent_picker_bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

bool IntentPickerBubbleView::Accept() {
  RunCallback(app_info_[selected_app_tag_].launch_name,
              app_info_[selected_app_tag_].type,
              chromeos::IntentPickerCloseReason::OPEN_APP,
              remember_selection_checkbox_->checked());
  return true;
}

bool IntentPickerBubbleView::Cancel() {
  RunCallback(arc::ArcIntentHelperBridge::kArcIntentHelperPackageName,
              apps::mojom::AppType::kUnknown,
              chromeos::IntentPickerCloseReason::STAY_IN_CHROME,
              remember_selection_checkbox_->checked());
  return true;
}

bool IntentPickerBubbleView::Close() {
  // Whenever closing the bubble without pressing |Just once| or |Always| we
  // need to report back that the user didn't select anything.
  RunCallback(kInvalidLaunchName, apps::mojom::AppType::kUnknown,
              chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED, false);
  return true;
}

bool IntentPickerBubbleView::ShouldShowCloseButton() const {
  return true;
}

void IntentPickerBubbleView::Init() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));

  // Creates a view to hold the views for each app.
  views::View* scrollable_view = new views::View();
  scrollable_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  size_t i = 0;
  size_t to_erase = app_info_.size();
  for (const auto& app_info : app_info_) {
    if (arc::ArcIntentHelperBridge::IsIntentHelperPackage(
            app_info.launch_name)) {
      to_erase = i;
      continue;
    }
    IntentPickerLabelButton* app_button = new IntentPickerLabelButton(
        this, &app_info.icon, app_info.launch_name, app_info.display_name);
    app_button->set_tag(i);
    scrollable_view->AddChildViewAt(app_button, i++);
  }

  // We should delete at most one entry, this is the case when Chrome is listed
  // as a candidate to handle a given URL.
  if (to_erase != app_info_.size())
    app_info_.erase(app_info_.begin() + to_erase);

  scroll_view_ = new views::ScrollView();
  scroll_view_->SetBackgroundColor(SK_ColorWHITE);
  scroll_view_->SetContents(scrollable_view);
  // This part gives the scroll a fixed width and height. The height depends on
  // how many app candidates we got and how many we actually want to show.
  // The added 0.5 on the else block allow us to let the user know there are
  // more than |kMaxAppResults| apps accessible by scrolling the list.
  size_t rows = GetScrollViewSize();
  if (rows <= kMaxAppResults) {
    scroll_view_->ClipHeightTo(kRowHeight, rows * kRowHeight);
  } else {
    scroll_view_->ClipHeightTo(kRowHeight, (kMaxAppResults + 0.5) * kRowHeight);
  }

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                kMaxWidth, 0);

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnSetId,
                              views::GridLayout::kFixedSize, kTitlePadding);
  layout->AddView(scroll_view_);
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId, 0);
  layout->AddView(CreateHorizontalSeparator());

  // This second ColumnSet has a padding column in order to manipulate the
  // Checkbox positioning freely.
  constexpr int kColumnSetIdPadded = 1;
  views::ColumnSet* cs_padded = layout->AddColumnSet(kColumnSetIdPadded);
  cs_padded->AddPaddingColumn(views::GridLayout::kFixedSize, kTitlePadding);
  cs_padded->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                       kMaxWidth - 2 * kTitlePadding, 0);

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnSetIdPadded,
                              views::GridLayout::kFixedSize, 0);
  remember_selection_checkbox_ = new views::Checkbox(l10n_util::GetStringUTF16(
      IDS_INTENT_PICKER_BUBBLE_VIEW_REMEMBER_SELECTION));
  layout->AddView(remember_selection_checkbox_);
  UpdateCheckboxState();

  layout->AddPaddingRow(views::GridLayout::kFixedSize, kTitlePadding);
}

base::string16 IntentPickerBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH);
}

bool IntentPickerBubbleView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (disable_stay_in_chrome_ && button == ui::DIALOG_BUTTON_CANCEL)
    return false;
  return true;
}

base::string16 IntentPickerBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_OK
          ? IDS_INTENT_PICKER_BUBBLE_VIEW_USE_APP
          : IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME);
}

IntentPickerBubbleView::IntentPickerBubbleView(
    std::vector<AppInfo> app_info,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents,
    bool disable_stay_in_chrome)
    : LocationBarBubbleDelegateView(nullptr /* anchor_view */,
                                    gfx::Point(),
                                    web_contents),
      intent_picker_cb_(std::move(intent_picker_cb)),
      selected_app_tag_(0),
      scroll_view_(nullptr),
      app_info_(std::move(app_info)),
      remember_selection_checkbox_(nullptr),
      disable_stay_in_chrome_(disable_stay_in_chrome) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::INTENT_PICKER);
}

IntentPickerBubbleView::~IntentPickerBubbleView() {
  SetLayoutManager(nullptr);
}

// If the widget gets closed without an app being selected we still need to use
// the callback so the caller can Resume the navigation.
void IntentPickerBubbleView::OnWidgetDestroying(views::Widget* widget) {
  RunCallback(kInvalidLaunchName, apps::mojom::AppType::kUnknown,
              chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED, false);
}

void IntentPickerBubbleView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  SetSelectedAppIndex(sender->tag(), &event);
  RequestFocus();
}

void IntentPickerBubbleView::ArrowButtonPressed(int index) {
  SetSelectedAppIndex(index, nullptr);
  AdjustScrollViewVisibleRegion();
}

void IntentPickerBubbleView::OnKeyEvent(ui::KeyEvent* event) {
  if (!IsKeyboardCodeArrow(event->key_code()) ||
      event->type() != ui::ET_KEY_RELEASED)
    return;

  int delta = 0;
  switch (event->key_code()) {
    case ui::VKEY_UP:
      delta = -1;
      break;
    case ui::VKEY_DOWN:
      delta = 1;
      break;
    case ui::VKEY_LEFT:
      delta = base::i18n::IsRTL() ? 1 : -1;
      break;
    case ui::VKEY_RIGHT:
      delta = base::i18n::IsRTL() ? -1 : 1;
      break;
    default:
      NOTREACHED();
      break;
  }
  ArrowButtonPressed(CalculateNextAppIndex(delta));

  View::OnKeyEvent(event);
}

IntentPickerLabelButton* IntentPickerBubbleView::GetIntentPickerLabelButtonAt(
    size_t index) {
  views::View* temp_contents = scroll_view_->contents();
  return static_cast<IntentPickerLabelButton*>(temp_contents->child_at(index));
}

bool IntentPickerBubbleView::HasCandidates() const {
  return app_info_.size() > 0;
}

void IntentPickerBubbleView::RunCallback(
    const std::string& launch_name,
    apps::mojom::AppType app_type,
    chromeos::IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (!intent_picker_cb_.is_null()) {
    // Calling Run() will make |intent_picker_cb_| null.
    std::move(intent_picker_cb_)
        .Run(launch_name, app_type, close_reason, should_persist);
  }

  intent_picker_bubble_ = nullptr;
}

size_t IntentPickerBubbleView::GetScrollViewSize() const {
  return scroll_view_->contents()->child_count();
}

void IntentPickerBubbleView::AdjustScrollViewVisibleRegion() {
  const views::ScrollBar* bar = scroll_view_->vertical_scroll_bar();
  if (bar) {
    scroll_view_->ScrollToPosition(const_cast<views::ScrollBar*>(bar),
                                   (selected_app_tag_ - 1) * kRowHeight);
  }
}

void IntentPickerBubbleView::SetSelectedAppIndex(int index,
                                                 const ui::Event* event) {
  // The selected app must be a value in the range [0, app_info_.size()-1].
  DCHECK(HasCandidates());
  DCHECK_LT(static_cast<size_t>(index), app_info_.size());
  DCHECK_GE(static_cast<size_t>(index), 0u);

  GetIntentPickerLabelButtonAt(selected_app_tag_)->MarkAsUnselected(nullptr);
  selected_app_tag_ = index;
  GetIntentPickerLabelButtonAt(selected_app_tag_)->MarkAsSelected(event);
  UpdateCheckboxState();
}

size_t IntentPickerBubbleView::CalculateNextAppIndex(int delta) {
  size_t size = GetScrollViewSize();
  return static_cast<size_t>((selected_app_tag_ + size + delta) % size);
}

void IntentPickerBubbleView::UpdateCheckboxState() {
  // TODO(crbug.com/826982): allow PWAs to have their decision persisted when
  // there is a central Chrome OS apps registry to store persistence.
  const bool should_enable =
      app_info_[selected_app_tag_].type != apps::mojom::AppType::kWeb;

  // Reset the checkbox state to the default unchecked if becomes disabled.
  if (!should_enable)
    remember_selection_checkbox_->SetChecked(false);
  remember_selection_checkbox_->SetEnabled(should_enable);
}

gfx::ImageSkia IntentPickerBubbleView::GetAppImageForTesting(size_t index) {
  return GetIntentPickerLabelButtonAt(index)->GetImage(
      views::Button::ButtonState::STATE_NORMAL);
}

views::InkDropState IntentPickerBubbleView::GetInkDropStateForTesting(
    size_t index) {
  return GetIntentPickerLabelButtonAt(index)->GetTargetInkDropState();
}

void IntentPickerBubbleView::PressButtonForTesting(size_t index,
                                                   const ui::Event& event) {
  views::Button* button =
      static_cast<views::Button*>(GetIntentPickerLabelButtonAt(index));
  ButtonPressed(button, event);
}
