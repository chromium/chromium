// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace {

constexpr char kInvalidLaunchName[] = "";

constexpr int kGridItemPreferredSize = 96;
constexpr int kGridItemsPerRow = 3;
constexpr int kGridInteriorColumnPadding = 8;
constexpr int kGridInteriorRowPadding = 8;
constexpr int kGridExteriorColumnPadding = 8;

constexpr int kGridItemTopInset = 12;
constexpr int kGridItemInset = 2;
constexpr int kGridItemInteriorPadding = 8;
constexpr int kGridItemBorderRadius = 4;
constexpr int kGridItemGroupId = 1;

bool g_auto_accept_intent_picker_bubble_for_testing = false;

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
}

bool IsDoubleClick(const ui::Event& event) {
  return (event.IsMouseEvent() && event.AsMouseEvent()->GetClickCount() == 2) ||
         (event.IsGestureEvent() &&
          event.AsGestureEvent()->details().tap_count() == 2);
}

// Callback for when an app is selected in the app list. First parameter is the
// index, second parameter is true if the dialog should be immediately accepted.
using AppSelectedCallback =
    base::RepeatingCallback<void(std::optional<size_t>, bool)>;

// Grid view:

// A Button which displays an app icon and name, as part of a grid layout of
// apps.
class IntentPickerAppGridButton : public views::Button {
  METADATA_HEADER(IntentPickerAppGridButton, views::Button)

 public:
  // Callback for when this app is selected. Parameter is true if the dialog
  // should be immediately accepted.
  using ButtonSelectedCallback = base::RepeatingCallback<void(bool)>;

  IntentPickerAppGridButton(ButtonSelectedCallback selected_callback,
                            const ui::ImageModel& icon_model,
                            const std::string& display_name)
      : views::Button(base::BindRepeating(&IntentPickerAppGridButton::OnPressed,
                                          base::Unretained(this))),
        selected_callback_(std::move(selected_callback)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::TLBR(kGridItemTopInset, kGridItemInset, kGridItemInset,
                          kGridItemInset),
        kGridItemInteriorPadding, true));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* icon_view =
        AddChildView(std::make_unique<views::ImageView>(icon_model));
    icon_view->SetCanProcessEventsWithinSubtree(false);

    auto* name_label = AddChildView(std::make_unique<views::Label>(
        base::UTF8ToUTF16(display_name), views::style::CONTEXT_BUTTON));
    name_label->SetMultiLine(true);
    name_label->SetMaxLines(2);
    name_label->SetMaximumWidth(kGridItemPreferredSize);
    name_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    name_label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);

    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kRadioButton);
    GetViewAccessibility().SetCheckedState(
        selected_ ? ax::mojom::CheckedState::kTrue
                  : ax::mojom::CheckedState::kFalse);
    // TODO(crbug.com/325137417): `SetName` should be called whenever the
    // `name_label` text changes, not just in the constructor.
    GetViewAccessibility().SetName(name_label->GetText());
    SetPreferredSize(gfx::Size(kGridItemPreferredSize, kGridItemPreferredSize));

    SetGroup(kGridItemGroupId);
  }
  IntentPickerAppGridButton(const IntentPickerAppGridButton&) = delete;
  IntentPickerAppGridButton& operator=(const IntentPickerAppGridButton&) =
      delete;
  ~IntentPickerAppGridButton() override = default;

  void SetSelected(bool selected) {
    selected_ = selected;
    UpdateBackground();
    GetViewAccessibility().SetCheckedState(
        selected_ ? ax::mojom::CheckedState::kTrue
                  : ax::mojom::CheckedState::kFalse);
  }

  // views::Button:
  void StateChanged(ButtonState old_state) override { UpdateBackground(); }

  bool IsGroupFocusTraversable() const override { return false; }
  views::View* GetSelectedViewForGroup(int group) override {
    if (group != kGridItemGroupId)
      return nullptr;

    Views siblings = parent()->children();
    auto it = base::ranges::find_if(siblings, [](views::View* v) {
      return views::AsViewClass<IntentPickerAppGridButton>(v)->selected_;
    });

    return it != siblings.end() ? *it : nullptr;
  }
  void OnFocus() override {
    Button::OnFocus();
    if (select_on_focus_)
      selected_callback_.Run(false);
  }
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    if (action_data.action == ax::mojom::Action::kFocus) {
      base::AutoReset<bool> reset(&select_on_focus_, false);
      RequestFocus();
      return true;
    }
    return Button::HandleAccessibleAction(action_data);
  }

 private:
  void UpdateBackground() {
    ui::ColorId color;
    if (selected_ || GetState() == ButtonState::STATE_PRESSED) {
      color = kColorIntentPickerItemBackgroundSelected;
    } else if (GetState() == ButtonState::STATE_HOVERED) {
      color = kColorIntentPickerItemBackgroundHovered;
    } else {
      SetBackground(nullptr);
      return;
    }

    SetBackground(
        views::CreateThemedRoundedRectBackground(color, kGridItemBorderRadius));
  }

  void OnPressed(const ui::Event& event) {
    bool should_open = IsDoubleClick(event) ||
                       (event.IsKeyEvent() &&
                        event.AsKeyEvent()->key_code() == ui::VKEY_RETURN);
    selected_callback_.Run(should_open);
  }

  bool selected_ = false;
  bool select_on_focus_ = true;
  ButtonSelectedCallback selected_callback_;
};

BEGIN_METADATA(IntentPickerAppGridButton)
END_METADATA

// Displays a list of apps as a grid of buttons.
class IntentPickerAppGridView
    : public IntentPickerBubbleView::IntentPickerAppsView {
  METADATA_HEADER(IntentPickerAppGridView,
                  IntentPickerBubbleView::IntentPickerAppsView)

 public:
  IntentPickerAppGridView(
      const std::vector<IntentPickerBubbleView::AppInfo>& apps,
      AppSelectedCallback selected_callback)
      : selected_callback_(selected_callback) {
    auto table_view = std::make_unique<views::TableLayoutView>();
    table_view->SetID(IntentPickerBubbleView::ViewId::kItemContainer);

    table_view->AddPaddingColumn(views::TableLayout::kFixedSize,
                                 kGridExteriorColumnPadding);
    for (int i = 0; i < kGridItemsPerRow; i++) {
      table_view->AddColumn(views::LayoutAlignment::kCenter,
                            views::LayoutAlignment::kStart,
                            views::TableLayout::kFixedSize,
                            views::TableLayout::ColumnSize::kUsePreferred,
                            /*fixed_width=*/0, /*min_width=*/0);
      if (i < kGridItemsPerRow - 1) {
        table_view->AddPaddingColumn(views::TableLayout::kFixedSize,
                                     kGridInteriorColumnPadding);
      }
    }
    table_view->AddPaddingColumn(views::TableLayout::kFixedSize,
                                 kGridExteriorColumnPadding);

    // Add padding to the exterior of the grid so that the focus ring on app
    // items is not clipped.
    constexpr int kFocusRingPadding = views::FocusRing::kDefaultHaloInset +
                                      views::FocusRing::kDefaultHaloThickness;

    int row_count = (apps.size() - 1) / kGridItemsPerRow + 1;
    table_view->AddPaddingRow(views::TableLayout::kFixedSize,
                              kFocusRingPadding);
    for (int i = 0; i < row_count; i++) {
      table_view->AddRows(1, views::TableLayout::kFixedSize);
      if (i < row_count - 1) {
        table_view->AddPaddingRow(views::TableLayout::kFixedSize,
                                  kGridInteriorRowPadding);
      }
    }
    table_view->AddPaddingRow(views::TableLayout::kFixedSize,
                              kFocusRingPadding);

    for (size_t i = 0; i < apps.size(); i++) {
      auto app_button = std::make_unique<IntentPickerAppGridButton>(
          base::BindRepeating(
              &IntentPickerAppGridView::SetSelectedIndexInternal,
              base::Unretained(this), i),
          apps[i].icon_model, apps[i].display_name);
      table_view->AddChildView(std::move(app_button));
    }

    SetContents(std::move(table_view));
    // Clip height so that at most two rows are visible, with a peek of the
    // third if it exists.
    ClipHeightTo(kGridItemPreferredSize, kGridItemPreferredSize * 2.5f);
  }

  void SetSelectedIndex(std::optional<size_t> index) override {
    SetSelectedIndexInternal(index, false);
  }

  std::optional<size_t> GetSelectedIndex() const override {
    return selected_app_index_;
  }

 private:
  void SetSelectedIndexInternal(std::optional<size_t> new_index,
                                bool accepted) {
    if (selected_app_index_.has_value()) {
      GetButtonAtIndex(selected_app_index_.value())->SetSelected(false);
    }
    if (new_index.has_value()) {
      GetButtonAtIndex(new_index.value())->SetSelected(true);
    }

    if (selected_app_index_.has_value() && new_index.has_value() &&
        GetButtonAtIndex(selected_app_index_.value())->HasFocus()) {
      GetButtonAtIndex(new_index.value())->RequestFocus();
    }

    selected_app_index_ = new_index;

    selected_callback_.Run(new_index, accepted);
  }

  IntentPickerAppGridButton* GetButtonAtIndex(size_t index) {
    const auto& children = contents()->children();
    return views::AsViewClass<IntentPickerAppGridButton>(children[index]);
  }

  AppSelectedCallback selected_callback_;

  std::optional<size_t> selected_app_index_ = 0;
};

BEGIN_METADATA(IntentPickerAppGridView)
ADD_PROPERTY_METADATA(std::optional<size_t>, SelectedIndex)
END_METADATA

// List view:

// A button that represents a candidate intent handler.
class IntentPickerLabelButton : public views::LabelButton {
  METADATA_HEADER(IntentPickerLabelButton, views::LabelButton)

 public:
  IntentPickerLabelButton(PressedCallback callback,
                          const ui::ImageModel& icon_model,
                          const std::string& display_name)
      : LabelButton(std::move(callback),
                    base::UTF8ToUTF16(std::string_view(display_name))) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!icon_model.IsEmpty())
      SetImageModel(views::ImageButton::STATE_NORMAL, icon_model);
    auto* provider = ChromeLayoutProvider::Get();
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
        provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI),
        provider->GetInsetsMetric(views::INSETS_DIALOG).left())));
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->SetBaseColorId(
        views::TypographyProvider::Get().GetColorId(
            views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY));
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
};

BEGIN_METADATA(IntentPickerLabelButton)
END_METADATA

class IntentPickerAppListView
    : public IntentPickerBubbleView::IntentPickerAppsView {
  METADATA_HEADER(IntentPickerAppListView,
                  IntentPickerBubbleView::IntentPickerAppsView)

 public:
  IntentPickerAppListView(
      const std::vector<IntentPickerBubbleView::AppInfo>& apps,
      AppSelectedCallback selected_callback)
      : selected_callback_(selected_callback) {
    auto scrollable_view = std::make_unique<views::View>();
    scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    scrollable_view->SetID(IntentPickerBubbleView::ViewId::kItemContainer);

    for (size_t i = 0; i < apps.size(); i++) {
      auto app_button = std::make_unique<IntentPickerLabelButton>(
          base::BindRepeating(&IntentPickerAppListView::OnAppPressed,
                              base::Unretained(this), i),
          apps[i].icon_model, apps[i].display_name);
      scrollable_view->AddChildViewAt(std::move(app_button), i);
    }

    SetBackgroundThemeColorId(ui::kColorBubbleBackground);
    SetContents(std::move(scrollable_view));
    DCHECK(!contents()->children().empty());
    const int row_height =
        contents()->children().front()->GetPreferredSize().height();
    // Use |kMaxAppResults| as a measure of how many apps we want to show.
    constexpr int kMaxAppResults = 3;
    ClipHeightTo(row_height, (kMaxAppResults + 0.5) * row_height);
  }

  ~IntentPickerAppListView() override = default;

  void SetSelectedIndex(std::optional<size_t> index) override {
    DCHECK(index.has_value());  // List-style intent picker does not support
                                // having no selection.
    SetSelectedAppIndex(index.value(), nullptr);
  }

  std::optional<size_t> GetSelectedIndex() const override {
    return selected_app_index_;
  }

  void OnKeyEvent(ui::KeyEvent* event) override {
    if (!IsKeyboardCodeArrow(event->key_code()) ||
        event->type() != ui::EventType::kKeyReleased) {
      return;
    }

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
    }

    SetSelectedAppIndex(CalculateNextAppIndex(delta), nullptr);
    AdjustScrollViewVisibleRegion();

    ScrollView::OnKeyEvent(event);
  }

 private:
  void OnAppPressed(size_t index, const ui::Event& event) {
    SetSelectedAppIndex(index, &event);
  }

  void SetSelectedAppIndex(size_t index, const ui::Event* event) {
    GetIntentPickerLabelButtonAt(selected_app_index_)
        ->MarkAsUnselected(nullptr);
    selected_app_index_ = index;
    GetIntentPickerLabelButtonAt(selected_app_index_)->MarkAsSelected(event);

    bool accepted = false;
    if (event && IsDoubleClick(*event)) {
      accepted = true;
    }

    selected_callback_.Run(index, accepted);
  }

  size_t CalculateNextAppIndex(int delta) {
    size_t size = contents()->children().size();
    return static_cast<size_t>((selected_app_index_ + delta) % size);
  }

  void AdjustScrollViewVisibleRegion() {
    views::ScrollBar* bar = vertical_scroll_bar();
    if (bar) {
      const int row_height =
          contents()->children().front()->GetPreferredSize().height();
      ScrollToPosition(bar, (selected_app_index_ - 1) * row_height);
    }
  }

  IntentPickerLabelButton* GetIntentPickerLabelButtonAt(size_t index) {
    const auto& children = contents()->children();
    DCHECK_LT(index, children.size());
    return views::AsViewClass<IntentPickerLabelButton>(children[index]);
  }

  AppSelectedCallback selected_callback_;

  size_t selected_app_index_ = 0;
};

BEGIN_METADATA(IntentPickerAppListView)
ADD_PROPERTY_METADATA(std::optional<size_t>, SelectedIndex)
END_METADATA

}  // namespace

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
    const std::optional<url::Origin>& initiating_origin,
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
  intent_picker_bubble_->Initialize();
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(intent_picker_bubble_);

  if (bubble_type == BubbleType::kClickToCall) {
    ClickToCallUiController::GetOrCreateFromWebContents(web_contents)
        ->ClearLastDialog();
  }

  DCHECK(intent_picker_bubble_->HasCandidates());
  intent_picker_bubble_->ShowForReason(DisplayReason::USER_GESTURE);

  intent_picker_bubble_->SelectDefaultItem();
  if (g_auto_accept_intent_picker_bubble_for_testing) {
    intent_picker_bubble_->AcceptDialog();
  }
  return widget;
}

// static
base::AutoReset<bool>
IntentPickerBubbleView::SetAutoAcceptIntentPickerBubbleForTesting() {
  return base::AutoReset<bool>(&g_auto_accept_intent_picker_bubble_for_testing,
                               true);
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
  auto selected_index = GetSelectedIndex();
  // Dialog cannot be accepted when there is no selection.
  DCHECK(selected_index.has_value());
  RunCallbackAndCloseBubble(app_info_[selected_index.value()].launch_name,
                            app_info_[selected_index.value()].type,
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

void IntentPickerBubbleView::SelectDefaultItem() {
  if (use_grid_view_ && app_info_.size() > 1) {
    apps_view_->SetSelectedIndex(std::nullopt);
    // The default button is disabled in this case. Clear the focus so it
    // returns to the window, as if there was no default button in the first
    // place.
    GetWidget()->GetFocusManager()->ClearFocus();
  } else {
    apps_view_->SetSelectedIndex(0);
  }
}

std::optional<size_t> IntentPickerBubbleView::GetSelectedIndex() const {
  return apps_view_->GetSelectedIndex();
}

std::u16string IntentPickerBubbleView::GetWindowTitle() const {
  if (bubble_type_ == BubbleType::kClickToCall) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
  }

  return l10n_util::GetStringUTF16(
      use_grid_view_ ? IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_IN_APP
                     : IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH);
}

IntentPickerBubbleView::IntentPickerBubbleView(
    views::View* anchor_view,
    BubbleType bubble_type,
    std::vector<AppInfo> app_info,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    const std::optional<url::Origin>& initiating_origin)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      intent_picker_cb_(std::move(intent_picker_cb)),
      app_info_(std::move(app_info)),
      use_grid_view_(apps::features::ShouldShowLinkCapturingUX() &&
                     bubble_type == BubbleType::kLinkCapturing),
      show_stay_in_chrome_(show_stay_in_chrome && !use_grid_view_),
      show_remember_selection_(show_remember_selection),
      bubble_type_(bubble_type),
      initiating_origin_(initiating_origin) {
  SetButtons(show_stay_in_chrome_
                 ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)
                 : static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          bubble_type_ == BubbleType::kClickToCall
              ? IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_CALL_BUTTON_LABEL
              : IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
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
  // Margins are manually added in Initialize().
  set_margins(gfx::Insets());
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

void IntentPickerBubbleView::OnAppSelected(std::optional<size_t> index,
                                           bool accepted) {
  SetButtonEnabled(ui::mojom::DialogButton::kOk, index.has_value());

  if (index.has_value()) {
    UpdateCheckboxState(index.value());
  }

  if (accepted) {
    DCHECK(index.has_value());
    AcceptDialog();
  }
}

void IntentPickerBubbleView::Initialize() {
  const bool show_origin =
      initiating_origin_ &&
      !initiating_origin_->IsSameOriginWith(
          web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  auto leading_content_type = use_grid_view_
                                  ? views::DialogContentType::kText
                                  : views::DialogContentType::kControl;
  auto trailing_content_type = (show_origin && !show_remember_selection_)
                                   ? views::DialogContentType::kText
                                   : views::DialogContentType::kControl;
  const auto* provider = ChromeLayoutProvider::Get();
  auto insets = provider->GetDialogInsetsForContentType(leading_content_type,
                                                        trailing_content_type);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(insets.top(), 0, insets.bottom(), 0),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  insets = gfx::Insets::TLBR(0, insets.left(), 0, insets.right());

  const int kMaxDialogWidth =
      provider->GetDistanceMetric(views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

  // Create a container for all of the individual app views.
  if (use_grid_view_) {
    apps_view_ = AddChildView(std::make_unique<IntentPickerAppGridView>(
        app_info_, base::BindRepeating(&IntentPickerBubbleView::OnAppSelected,
                                       base::Unretained(this))));
  } else {
    apps_view_ = AddChildView(std::make_unique<IntentPickerAppListView>(
        app_info_, base::BindRepeating(&IntentPickerBubbleView::OnAppSelected,
                                       base::Unretained(this))));
  }

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
    label->SetMaximumWidth(kMaxDialogWidth - insets.width());
    label->SetProperty(views::kMarginsKey, insets);
  }

  if (show_remember_selection_) {
    if (!use_grid_view_) {
      AddChildView(std::make_unique<views::Separator>());
    }

    remember_selection_checkbox_ = AddChildView(
        std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
            IDS_INTENT_PICKER_BUBBLE_VIEW_REMEMBER_SELECTION)));
    remember_selection_checkbox_->SetID(ViewId::kRememberCheckbox);
    remember_selection_checkbox_->SetProperty(views::kMarginsKey, insets);
  }
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
    DCHECK(!should_persist || !launch_name.empty());
    std::move(intent_picker_cb_)
        .Run(launch_name, entry_type, close_reason, should_persist);
  }
}

void IntentPickerBubbleView::UpdateCheckboxState(size_t index) {
  if (!remember_selection_checkbox_)
    return;
  auto selected_app_type = app_info_[index].type;
  bool should_enable = selected_app_type != apps::PickerEntryType::kDevice;

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

BEGIN_METADATA(IntentPickerBubbleView)
END_METADATA

BEGIN_METADATA(IntentPickerBubbleView, IntentPickerAppsView)
END_METADATA
