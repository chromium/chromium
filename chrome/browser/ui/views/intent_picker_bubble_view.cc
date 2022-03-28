// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <utility>

#include "base/bind.h"
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
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kInvalidLaunchName[] = "";

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
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
    if (!icon_model.IsEmpty())
      SetImageModel(views::ImageButton::STATE_NORMAL, icon_model);
    auto* provider = ChromeLayoutProvider::Get();
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
        provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI),
        provider->GetInsetsMetric(views::INSETS_DIALOG).left())));
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->SetBaseColorCallback(
        base::BindRepeating(&HoverButton::GetInkDropColor, this));
  }
  IntentPickerLabelButton(const IntentPickerLabelButton&) = delete;
  IntentPickerLabelButton& operator=(const IntentPickerLabelButton&) = delete;
  ~IntentPickerLabelButton() override = default;

  void MarkAsUnselected(const ui::Event* event) {
    views::InkDrop::Get(this)->AnimateToState(
        views::InkDropState::HIDDEN, ui::LocatedEvent::FromIfValid(event));
  }

  void MarkAsSelected(const ui::Event* event) {
    views::InkDrop::Get(this)->AnimateToState(
        views::InkDropState::ACTIVATED, ui::LocatedEvent::FromIfValid(event));
  }

  views::InkDropState GetTargetInkDropState() {
    return views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState();
  }
};

BEGIN_METADATA(IntentPickerLabelButton, views::LabelButton)
END_METADATA

// static
IntentPickerBubbleView* IntentPickerBubbleView::intent_picker_bubble_ = nullptr;

// static
views::Widget* IntentPickerBubbleView::ShowBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    BubbleType bubble_type,
    content::WebContents* web_contents,
    std::vector<AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    const absl::optional<url::Origin>& initiating_origin,
    IntentPickerResponse intent_picker_cb) {
  if (intent_picker_bubble_) {
    intent_picker_bubble_->CloseBubble();
  }
  intent_picker_bubble_ = new IntentPickerBubbleView(
      anchor_view, bubble_type, std::move(app_info),
      std::move(intent_picker_cb), web_contents, show_stay_in_chrome,
      show_remember_selection, initiating_origin);
  if (highlighted_button)
    intent_picker_bubble_->SetHighlightedButton(highlighted_button);
  intent_picker_bubble_->set_margins(gfx::Insets());
  intent_picker_bubble_->Initialize();
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(intent_picker_bubble_);
  // TODO(aleventhal) Should not need to be focusable as only descendant widgets
  // are interactive; however, it does call RequestFocus(). If it is going to be
  // focusable, it needs an accessible name so that it can pass accessibility
  // checks. Use the same accessible name as the icon. Set the role as kDialog
  // to ensure screen readers immediately announce the text of this view.
  intent_picker_bubble_->GetViewAccessibility().OverrideRole(
      ax::mojom::Role::kDialog);
  if (bubble_type == BubbleType::kClickToCall) {
    intent_picker_bubble_->GetViewAccessibility().OverrideName(
        l10n_util::GetStringUTF16(
            IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL));
    ClickToCallUiController::GetOrCreateFromWebContents(web_contents)
        ->ClearLastDialog();
  } else {
    intent_picker_bubble_->GetViewAccessibility().OverrideName(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON));
  }
  intent_picker_bubble_->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  DCHECK(intent_picker_bubble_->HasCandidates());
  widget->Show();
  intent_picker_bubble_->GetIntentPickerLabelButtonAt(0)->MarkAsSelected(
      nullptr);
  return widget;
}

// static
std::unique_ptr<IntentPickerBubbleView>
IntentPickerBubbleView::CreateBubbleViewForTesting(
    views::View* anchor_view,
    BubbleType bubble_type,
    std::vector<AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    const absl::optional<url::Origin>& initiating_origin,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents) {
  auto bubble = std::make_unique<IntentPickerBubbleView>(
      anchor_view, bubble_type, std::move(app_info),
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
  ClearIntentPickerBubbleView();
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
  const char* launch_name = apps_util::kUseBrowserForLink;
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
  if (bubble_type_ == BubbleType::kClickToCall) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
  }

  return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH);
}

IntentPickerBubbleView::IntentPickerBubbleView(
    views::View* anchor_view,
    BubbleType bubble_type,
    std::vector<AppInfo> app_info,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    const absl::optional<url::Origin>& initiating_origin)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      intent_picker_cb_(std::move(intent_picker_cb)),
      app_info_(std::move(app_info)),
      show_stay_in_chrome_(show_stay_in_chrome),
      show_remember_selection_(show_remember_selection),
      bubble_type_(bubble_type),
      initiating_origin_(initiating_origin) {
  SetButtons(show_stay_in_chrome_
                 ? (ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL)
                 : ui::DIALOG_BUTTON_OK);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          bubble_type_ == BubbleType::kClickToCall
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
  SetCloseOnMainFrameOriginNavigation(bubble_type == BubbleType::kClickToCall);

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
  // Creates a view to hold the views for each app.
  auto scrollable_view = std::make_unique<views::View>();
  scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  size_t i = 0;
  size_t to_erase = app_info_.size();
  for (const auto& app_info : app_info_) {
#if BUILDFLAG(IS_CHROMEOS)
    if (app_info.launch_name == arc::kArcIntentHelperPackageName) {
      to_erase = i;
      continue;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
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
  scroll_view->SetBackgroundThemeColorId(ui::kColorBubbleBackground);
  scroll_view->SetContents(std::move(scrollable_view));
  DCHECK(!scroll_view->contents()->children().empty());
  const int row_height =
      scroll_view->contents()->children().front()->GetPreferredSize().height();
  // TODO(djacobo): Replace this limit to correctly reflect the UI mocks, which
  // now instead of limiting the results to 3.5 will allow whatever fits in
  // 256pt. Using |kMaxAppResults| as a measure of how many apps we want to
  // show.
  scroll_view->ClipHeightTo(row_height,
                            (apps::kMaxAppResults + 0.5) * row_height);

  const bool show_origin =
      initiating_origin_ &&
      !initiating_origin_->IsSameOriginWith(
          web_contents()->GetMainFrame()->GetLastCommittedOrigin());

  const auto* provider = ChromeLayoutProvider::Get();
  auto insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl,
      (show_origin && !show_remember_selection_)
          ? views::DialogContentType::kText
          : views::DialogContentType::kControl);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(insets.top(), 0, insets.bottom(), 0),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  insets = gfx::Insets::TLBR(0, insets.left(), 0, insets.right());

  scroll_view_ = AddChildView(std::move(scroll_view));

  if (show_origin) {
    std::u16string origin_text = l10n_util::GetStringFUTF16(
        bubble_type_ == BubbleType::kClickToCall
            ? IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_INITIATING_ORIGIN
            : IDS_INTENT_PICKER_BUBBLE_VIEW_INITIATING_ORIGIN,
        url_formatter::FormatOriginForSecurityDisplay(*initiating_origin_));
    auto* label = AddChildView(std::make_unique<views::Label>(
        origin_text, ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
        views::style::STYLE_SECONDARY));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetAllowCharacterBreak(true);
    label->SetMultiLine(true);
    constexpr int kMaxDialogWidth = 320;
    label->SetMaximumWidth(kMaxDialogWidth - insets.width());
    label->SetProperty(views::kMarginsKey, insets);
  }

  if (show_remember_selection_) {
    AddChildView(std::make_unique<views::Separator>());

    remember_selection_checkbox_ = AddChildView(
        std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
            IDS_INTENT_PICKER_BUBBLE_VIEW_REMEMBER_SELECTION)));
    remember_selection_checkbox_->SetProperty(views::kMarginsKey, insets);
    UpdateCheckboxState();
  }
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
  ClearIntentPickerBubbleView();
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
}

size_t IntentPickerBubbleView::GetScrollViewSize() const {
  return scroll_view_->contents()->children().size();
}

void IntentPickerBubbleView::AdjustScrollViewVisibleRegion() {
  const views::ScrollBar* bar = scroll_view_->vertical_scroll_bar();
  if (bar) {
    const int row_height = scroll_view_->contents()
                               ->children()
                               .front()
                               ->GetPreferredSize()
                               .height();
    scroll_view_->ScrollToPosition(const_cast<views::ScrollBar*>(bar),
                                   (selected_app_tag_ - 1) * row_height);
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
  auto selected_app_type = app_info_[selected_app_tag_].type;
  bool should_enable = true;
  if (selected_app_type == apps::PickerEntryType::kDevice) {
    // TODO(crbug.com/1000037): Allow persisting remote devices.
    should_enable = false;
  } else if (selected_app_type == apps::PickerEntryType::kWeb) {
    should_enable = apps::IntentPickerPwaPersistenceEnabled();
  }

  // Reset the checkbox state to the default unchecked if becomes disabled.
  if (!should_enable)
    remember_selection_checkbox_->SetChecked(false);
  remember_selection_checkbox_->SetEnabled(should_enable);
}

void IntentPickerBubbleView::ClearIntentPickerBubbleView() {
  // This is called asynchronously during OnWidgetDestroying, at which point
  // intent_picker_bubble_ may have already been cleared or set to something
  // else.
  if (intent_picker_bubble_ == this)
    intent_picker_bubble_ = nullptr;
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
