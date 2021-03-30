// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/intent_helper/intent_picker_constants.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
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
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// TODO(djacobo): Replace this limit to correctly reflect the UI mocks, which
// now instead of limiting the results to 3.5 will allow whatever fits in 256pt.
// Using |kMaxAppResults| as a measure of how many apps we want to show.
constexpr size_t kMaxAppResults = apps::kMaxAppResults;
// Main components sizes
constexpr int kTitlePadding = 16;
constexpr int kRowHeight = 32;
constexpr int kMaxIntentPickerLabelButtonWidth = 320;
constexpr gfx::Insets kSeparatorPadding(16, 0, 16, 0);
constexpr SkColor kSeparatorColor = SkColorSetARGB(0x1F, 0x0, 0x0, 0x0);

constexpr char kInvalidLaunchName[] = "";

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
}

std::unique_ptr<views::Separator> CreateHorizontalSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(kSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(kSeparatorPadding));
  return separator;
}

// Creates a label that is identical to CreateFrontElidingTitleLabel but has a
// different style as it is not shown as a title label.
std::unique_ptr<views::View> CreateOriginView(const url::Origin& origin,
                                              int text_id) {
  std::u16string origin_text = l10n_util::GetStringFUTF16(
      text_id, url_formatter::FormatOriginForSecurityDisplay(origin));
  auto label = std::make_unique<views::Label>(
      origin_text, ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  label->SetMultiLine(false);
  return label;
}

}  // namespace

// IntentPickerLabelButton

// A button that represents a candidate intent handler.
class IntentPickerLabelButton : public views::LabelButton {
 public:
  METADATA_HEADER(IntentPickerLabelButton);

  IntentPickerLabelButton(PressedCallback callback,
                          const ui::ImageModel& icon_model,
                          const std::string& display_name)
      : LabelButton(std::move(callback),
                    base::UTF8ToUTF16(base::StringPiece(display_name))) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetMinSize(gfx::Size(kMaxIntentPickerLabelButtonWidth, kRowHeight));
    SetInkDropMode(InkDropMode::ON);
    if (!icon_model.IsEmpty()) {
      SetImageModel(views::ImageButton::STATE_NORMAL, icon_model);
    }
    SetBorder(views::CreateEmptyBorder(8, 16, 8, 0));
    SetInkDropBaseColor(SK_ColorGRAY);
    SetInkDropVisibleOpacity(kToolbarInkDropVisibleOpacity);
  }
  IntentPickerLabelButton(const IntentPickerLabelButton&) = delete;
  IntentPickerLabelButton& operator=(const IntentPickerLabelButton&) = delete;
  ~IntentPickerLabelButton() override = default;

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
};

BEGIN_METADATA(IntentPickerLabelButton, views::LabelButton)
END_METADATA

// static
IntentPickerBubbleView* IntentPickerBubbleView::intent_picker_bubble_ = nullptr;

// static
views::Widget* IntentPickerBubbleView::ShowBubble(
    views::View* anchor_view,
    PageActionIconView* icon_view,
    PageActionIconType icon_type,
    content::WebContents* web_contents,
    std::vector<AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    const base::Optional<url::Origin>& initiating_origin,
    IntentPickerResponse intent_picker_cb) {
  if (intent_picker_bubble_) {
    intent_picker_bubble_->CloseBubble();
    intent_picker_bubble_ = nullptr;
  }
  intent_picker_bubble_ = new IntentPickerBubbleView(
      anchor_view, icon_view, icon_type, std::move(app_info),
      std::move(intent_picker_cb), web_contents, show_stay_in_chrome,
      show_remember_selection, initiating_origin);
  if (icon_view)
    intent_picker_bubble_->SetHighlightedButton(icon_view);
  intent_picker_bubble_->set_margins(gfx::Insets());
  intent_picker_bubble_->Initialize();
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(intent_picker_bubble_);
  // TODO(ellyjones): It should not at all be necessary to call Layout() here;
  // it should have just happened during ::CreateBubble(). Figure out why this
  // is here and/or simply delete it.
  intent_picker_bubble_->GetWidget()->GetRootView()->Layout();
  // TODO(aleventhal) Should not need to be focusable as only descendant widgets
  // are interactive; however, it does call RequestFocus(). If it is going to be
  // focusable, it needs an accessible name so that it can pass accessibility
  // checks. Use the same accessible name as the icon. Set the role as kDialog
  // to ensure screen readers immediately announce the text of this view.
  intent_picker_bubble_->GetViewAccessibility().OverrideRole(
      ax::mojom::Role::kDialog);
  if (icon_type == PageActionIconType::kClickToCall) {
    intent_picker_bubble_->GetViewAccessibility().OverrideName(
        l10n_util::GetStringUTF16(
            IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL));
    ClickToCallUiController::GetOrCreateFromWebContents(web_contents)
        ->ClearLastDialog();
  } else {
    DCHECK(icon_type == PageActionIconType::kIntentPicker);
    intent_picker_bubble_->GetViewAccessibility().OverrideName(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON));
  }
  intent_picker_bubble_->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  DCHECK(intent_picker_bubble_->HasCandidates());
  intent_picker_bubble_->GetIntentPickerLabelButtonAt(0)->MarkAsSelected(
      nullptr);
  widget->Show();
  return widget;
}

// static
std::unique_ptr<IntentPickerBubbleView>
IntentPickerBubbleView::CreateBubbleViewForTesting(
    views::View* anchor_view,
    PageActionIconView* icon_view,
    PageActionIconType icon_type,
    std::vector<AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    const base::Optional<url::Origin>& initiating_origin,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents) {
  auto bubble = std::make_unique<IntentPickerBubbleView>(
      anchor_view, icon_view, icon_type, std::move(app_info),
      std::move(intent_picker_cb), web_contents, show_stay_in_chrome,
      show_remember_selection, initiating_origin);
  bubble->Initialize();
  return bubble;
}

// static
void IntentPickerBubbleView::CloseCurrentBubble() {
  if (intent_picker_bubble_)
    intent_picker_bubble_->CloseBubble();
}

void IntentPickerBubbleView::CloseBubble() {
  ClearBubbleView();
  LocationBarBubbleDelegateView::CloseBubble();
}

void IntentPickerBubbleView::OnDialogAccepted() {
  bool should_persist = remember_selection_checkbox_ &&
                        remember_selection_checkbox_->GetChecked();
  RunCallbackAndCloseBubble(app_info_[selected_app_tag_].launch_name,
                            app_info_[selected_app_tag_].type,
                            apps::IntentPickerCloseReason::OPEN_APP,
                            should_persist);
}

void IntentPickerBubbleView::OnDialogCancelled() {
  const char* launch_name = apps::kUseBrowserForLink;
  bool should_persist = remember_selection_checkbox_ &&
                        remember_selection_checkbox_->GetChecked();
  RunCallbackAndCloseBubble(launch_name, apps::PickerEntryType::kUnknown,
                            apps::IntentPickerCloseReason::STAY_IN_CHROME,
                            should_persist);
}

void IntentPickerBubbleView::OnDialogClosed() {
  // Whenever closing the bubble without pressing |Just once| or |Always| we
  // need to report back that the user didn't select anything.
  RunCallbackAndCloseBubble(kInvalidLaunchName, apps::PickerEntryType::kUnknown,
                            apps::IntentPickerCloseReason::DIALOG_DEACTIVATED,
                            false);
}

bool IntentPickerBubbleView::ShouldShowCloseButton() const {
  return true;
}

std::u16string IntentPickerBubbleView::GetWindowTitle() const {
  if (icon_type_ == PageActionIconType::kClickToCall) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
  }

  DCHECK(icon_type_ == PageActionIconType::kIntentPicker);
  return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH);
}

IntentPickerBubbleView::IntentPickerBubbleView(
    views::View* anchor_view,
    PageActionIconView* icon_view,
    PageActionIconType icon_type,
    std::vector<AppInfo> app_info,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    const base::Optional<url::Origin>& initiating_origin)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      intent_picker_cb_(std::move(intent_picker_cb)),
      selected_app_tag_(0),
      app_info_(std::move(app_info)),
      show_stay_in_chrome_(show_stay_in_chrome),
      show_remember_selection_(show_remember_selection),
      icon_view_(icon_view),
      icon_type_(icon_type),
      initiating_origin_(initiating_origin) {
  SetButtons(show_stay_in_chrome_
                 ? (ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL)
                 : ui::DIALOG_BUTTON_OK);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          icon_type_ == PageActionIconType::kClickToCall
              ? IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_CALL_BUTTON_LABEL
              : IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME));
  SetAcceptCallback(base::BindOnce(&IntentPickerBubbleView::OnDialogAccepted,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&IntentPickerBubbleView::OnDialogCancelled,
                                   base::Unretained(this)));
  SetCloseCallback(base::BindOnce(&IntentPickerBubbleView::OnDialogClosed,
                                  base::Unretained(this)));

  // Click to call bubbles need to be closed after navigation if the main frame
  // origin changed. Other intent picker bubbles will be handled in
  // intent_picker_helpers, they will get closed on each navigation start and
  // should stay open until after navigation finishes.
  SetCloseOnMainFrameOriginNavigation(icon_type ==
                                      PageActionIconType::kClickToCall);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::INTENT_PICKER);
}

IntentPickerBubbleView::~IntentPickerBubbleView() {
  SetLayoutManager(nullptr);
}

// If the widget gets closed without an app being selected we still need to use
// the callback so the caller can Resume the navigation.
void IntentPickerBubbleView::OnWidgetDestroying(views::Widget* widget) {
  RunCallbackAndCloseBubble(kInvalidLaunchName, apps::PickerEntryType::kUnknown,
                            apps::IntentPickerCloseReason::DIALOG_DEACTIVATED,
                            false);
}

void IntentPickerBubbleView::AppButtonPressed(size_t index,
                                              const ui::Event& event) {
  SetSelectedAppIndex(index, &event);
  RequestFocus();
  if ((event.IsMouseEvent() && event.AsMouseEvent()->GetClickCount() == 2) ||
      (event.IsGestureEvent() &&
       event.AsGestureEvent()->details().tap_count() == 2)) {
    AcceptDialog();
  }
}

void IntentPickerBubbleView::ArrowButtonPressed(size_t index) {
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

void IntentPickerBubbleView::Initialize() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Creates a view to hold the views for each app.
  auto scrollable_view = std::make_unique<views::View>();
  scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  size_t i = 0;
  size_t to_erase = app_info_.size();
  for (const auto& app_info : app_info_) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (arc::ArcIntentHelperBridge::IsIntentHelperPackage(
            app_info.launch_name)) {
      to_erase = i;
      continue;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    auto app_button = std::make_unique<IntentPickerLabelButton>(
        base::BindRepeating(&IntentPickerBubbleView::AppButtonPressed,
                            base::Unretained(this), i),
        app_info.icon_model, app_info.display_name);
    scrollable_view->AddChildViewAt(std::move(app_button), i++);
  }

  // We should delete at most one entry, this is the case when Chrome is listed
  // as a candidate to handle a given URL.
  if (to_erase != app_info_.size())
    app_info_.erase(app_info_.begin() + to_erase);

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetBackgroundThemeColorId(
      ui::NativeTheme::kColorId_BubbleBackground);
  scroll_view->SetContents(std::move(scrollable_view));
  // This part gives the scroll a fixed width and height. The height depends on
  // how many app candidates we got and how many we actually want to show.
  // The added 0.5 on the else block allow us to let the user know there are
  // more than |kMaxAppResults| apps accessible by scrolling the list.
  scroll_view->ClipHeightTo(kRowHeight, (kMaxAppResults + 0.5) * kRowHeight);

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kFixed,
                kMaxIntentPickerLabelButtonWidth, 0);

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnSetId,
                              views::GridLayout::kFixedSize, kTitlePadding);
  scroll_view_ = layout->AddView(std::move(scroll_view));

  if (initiating_origin_ &&
      !initiating_origin_->IsSameOriginWith(
          web_contents()->GetMainFrame()->GetLastCommittedOrigin())) {
    constexpr int kColumnSetIdOrigin = 1;
    views::ColumnSet* cs_origin = layout->AddColumnSet(kColumnSetIdOrigin);
    cs_origin->AddPaddingColumn(views::GridLayout::kFixedSize, kTitlePadding);
    cs_origin->AddColumn(
        views::GridLayout::FILL, views::GridLayout::CENTER,
        views::GridLayout::kFixedSize, views::GridLayout::ColumnSize::kFixed,
        kMaxIntentPickerLabelButtonWidth - 2 * kTitlePadding, 0);

    layout->StartRowWithPadding(views::GridLayout::kFixedSize,
                                kColumnSetIdOrigin,
                                views::GridLayout::kFixedSize, kTitlePadding);

    layout->AddView(CreateOriginView(
        *initiating_origin_,
        icon_type_ == PageActionIconType::kClickToCall
            ? IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_INITIATING_ORIGIN
            : IDS_INTENT_PICKER_BUBBLE_VIEW_INITIATING_ORIGIN));
  }

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId, 0);

  if (show_remember_selection_) {
    layout->AddView(CreateHorizontalSeparator());

    // This second ColumnSet has a padding column in order to manipulate the
    // Checkbox positioning freely.
    constexpr int kColumnSetIdPadded = 2;
    views::ColumnSet* cs_padded = layout->AddColumnSet(kColumnSetIdPadded);
    cs_padded->AddPaddingColumn(views::GridLayout::kFixedSize, kTitlePadding);
    cs_padded->AddColumn(
        views::GridLayout::FILL, views::GridLayout::CENTER,
        views::GridLayout::kFixedSize, views::GridLayout::ColumnSize::kFixed,
        kMaxIntentPickerLabelButtonWidth - 2 * kTitlePadding, 0);

    layout->StartRowWithPadding(views::GridLayout::kFixedSize,
                                kColumnSetIdPadded,
                                views::GridLayout::kFixedSize, 0);

    remember_selection_checkbox_ = layout->AddView(
        std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
            IDS_INTENT_PICKER_BUBBLE_VIEW_REMEMBER_SELECTION)));
    UpdateCheckboxState();
  }
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kTitlePadding);
}

IntentPickerLabelButton* IntentPickerBubbleView::GetIntentPickerLabelButtonAt(
    size_t index) {
  const auto& children = scroll_view_->contents()->children();
  return static_cast<IntentPickerLabelButton*>(children[index]);
}

bool IntentPickerBubbleView::HasCandidates() const {
  return !app_info_.empty();
}

void IntentPickerBubbleView::RunCallbackAndCloseBubble(
    const std::string& launch_name,
    apps::PickerEntryType entry_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (!intent_picker_cb_.is_null()) {
    // Calling Run() will make |intent_picker_cb_| null.
    // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
    // after finding out the root cause.
    if (should_persist && launch_name.empty()) {
      base::debug::DumpWithoutCrashing();
    }
    std::move(intent_picker_cb_)
        .Run(launch_name, entry_type, close_reason, should_persist);
  }

  ClearBubbleView();
}

size_t IntentPickerBubbleView::GetScrollViewSize() const {
  return scroll_view_->contents()->children().size();
}

void IntentPickerBubbleView::AdjustScrollViewVisibleRegion() {
  const views::ScrollBar* bar = scroll_view_->vertical_scroll_bar();
  if (bar) {
    scroll_view_->ScrollToPosition(const_cast<views::ScrollBar*>(bar),
                                   (selected_app_tag_ - 1) * kRowHeight);
  }
}

void IntentPickerBubbleView::SetSelectedAppIndex(size_t index,
                                                 const ui::Event* event) {
  DCHECK(HasCandidates());
  DCHECK_LT(index, app_info_.size());

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
  if (!remember_selection_checkbox_)
    return;
  // TODO(crbug.com/826982): allow PWAs to have their decision persisted when
  // there is a central Chrome OS apps registry to store persistence.
  // TODO(crbug.com/1000037): allow to persist remote devices too.
  bool should_enable = false;
  if (base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence)) {
    should_enable = true;
  } else {
    auto selected_app_type = app_info_[selected_app_tag_].type;
    should_enable = selected_app_type != apps::PickerEntryType::kWeb &&
                    selected_app_type != apps::PickerEntryType::kDevice;
  }
  // Reset the checkbox state to the default unchecked if becomes disabled.
  if (!should_enable)
    remember_selection_checkbox_->SetChecked(false);
  remember_selection_checkbox_->SetEnabled(should_enable);
}

void IntentPickerBubbleView::ClearBubbleView() {
  intent_picker_bubble_ = nullptr;
  if (icon_view_)
    icon_view_->Update();
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
  AppButtonPressed(index, event);
}

BEGIN_METADATA(IntentPickerBubbleView, LocationBarBubbleDelegateView)
END_METADATA
