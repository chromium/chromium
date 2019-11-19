// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"

namespace payments {

namespace {

// This event is used to pass to ButtonPressed when its event parameter doesn't
// matter, only the sender.
class DummyEvent : public ui::Event {
 public:
  DummyEvent() : ui::Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

// This class is the actual sheet that gets pushed on the view_stack_. It
// implements views::FocusTraversable to trap focus within its hierarchy. This
// way, focus doesn't leave the topmost sheet on the view stack to go on views
// that happen to be underneath.
// This class also overrides RequestFocus() to allow consumers to specify which
// view should be focused first (through SetFirstFocusableView). If no initial
// view is specified, the first view added to the hierarchy will get focus when
// this SheetView's RequestFocus() is called.
class SheetView : public views::View, public views::FocusTraversable {
 public:
  explicit SheetView(
      const base::Callback<void(bool*)>& enter_key_accelerator_callback)
      : first_focusable_(nullptr),
        focus_search_(std::make_unique<views::FocusSearch>(this, true, false)),
        enter_key_accelerator_(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)),
        enter_key_accelerator_callback_(enter_key_accelerator_callback) {
    if (enter_key_accelerator_callback_)
      AddAccelerator(enter_key_accelerator_);
  }

  // Sets |view| as the first focusable view in this pane. If it's nullptr, the
  // fallback is to use focus_search_ to find the first focusable view.
  void SetFirstFocusableView(views::View* view) { first_focusable_ = view; }

 private:
  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override { return focus_search_.get(); }

  views::FocusTraversable* GetFocusTraversableParent() override {
    return parent()->GetFocusTraversable();
  }

  views::View* GetFocusTraversableParentView() override { return this; }

  // views::View:
  views::FocusTraversable* GetPaneFocusTraversable() override { return this; }

  void RequestFocus() override {
    // In accessibility contexts, we want to focus the title of the sheet.
    views::View* title =
        GetViewByID(static_cast<int>(DialogViewID::SHEET_TITLE));
    views::FocusManager* focus = GetFocusManager();
    DCHECK(focus);

    title->RequestFocus();

    // RequestFocus only works if we are in an accessible context, and is a
    // no-op otherwise. Thus, if the focused view didn't change, we need to
    // proceed with setting the focus for standard usage.
    if (focus->GetFocusedView() == title)
      return;

    views::View* first_focusable = first_focusable_;

    if (!first_focusable) {
      views::FocusTraversable* dummy_focus_traversable;
      views::View* dummy_focus_traversable_view;
      first_focusable = focus_search_->FindNextFocusableView(
          nullptr, views::FocusSearch::SearchDirection::kForwards,
          views::FocusSearch::TraversalDirection::kDown,
          views::FocusSearch::StartingViewPolicy::kSkipStartingView,
          views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
          &dummy_focus_traversable, &dummy_focus_traversable_view);
    }

    if (first_focusable)
      first_focusable->RequestFocus();
  }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator == enter_key_accelerator_ &&
        enter_key_accelerator_callback_) {
      bool is_enabled = false;
      enter_key_accelerator_callback_.Run(&is_enabled);
      return is_enabled;
    }
    return views::View::AcceleratorPressed(accelerator);
  }

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    if (!details.is_add && details.child == first_focusable_)
      first_focusable_ = nullptr;
  }

  views::View* first_focusable_;
  std::unique_ptr<views::FocusSearch> focus_search_;
  ui::Accelerator enter_key_accelerator_;
  base::Callback<void(bool*)> enter_key_accelerator_callback_;

  DISALLOW_COPY_AND_ASSIGN(SheetView);
};

// A scroll view that displays a separator on the bounds where content is
// scrolled out of view. For example, if the view can be scrolled up to reveal
// more content, the top of the content area will display a separator.
class BorderedScrollView : public views::ScrollView {
 public:
  // The painter used by the scroll view to display the border.
  class BorderedScrollViewBorderPainter : public views::Painter {
   public:
    BorderedScrollViewBorderPainter(SkColor color,
                                    BorderedScrollView* scroll_view)
        : color_(color), scroll_view_(scroll_view) {}
    ~BorderedScrollViewBorderPainter() override {}

   private:
    // views::Painter:
    gfx::Size GetMinimumSize() const override { return gfx::Size(0, 2); }

    void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
      if (scroll_view_->HasTopBorder()) {
        canvas->Draw1pxLine(gfx::PointF(0, 0), gfx::PointF(size.width(), 0),
                            color_);
      }

      if (scroll_view_->HasBottomBorder()) {
        canvas->Draw1pxLine(gfx::PointF(0, size.height() - 1),
                            gfx::PointF(size.width(), size.height() - 1),
                            color_);
      }
    }

   private:
    SkColor color_;
    // The scroll view that owns the border that owns this painter.
    BorderedScrollView* scroll_view_;
    DISALLOW_COPY_AND_ASSIGN(BorderedScrollViewBorderPainter);
  };

  BorderedScrollView() : views::ScrollView() {
    SetBackground(views::CreateThemedSolidBackground(
        this, ui::NativeTheme::kColorId_DialogBackground));
    SetBorder(views::CreateBorderPainter(
        std::make_unique<BorderedScrollViewBorderPainter>(
            GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_SeparatorColor),
            this),
        gfx::Insets(1, 0, 1, 0)));
  }

  bool HasTopBorder() const {
    gfx::Rect visible_rect = GetVisibleRect();
    return visible_rect.y() > 0;
  }

  bool HasBottomBorder() const {
    gfx::Rect visible_rect = GetVisibleRect();
    return visible_rect.y() + visible_rect.height() < contents()->height();
  }

  // views::ScrollView:
  void ScrollToPosition(views::ScrollBar* source, int position) override {
    views::ScrollView::ScrollToPosition(source, position);
    SchedulePaint();
  }
};

}  // namespace

PaymentRequestSheetController::PaymentRequestSheetController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : spec_(spec), state_(state), dialog_(dialog) {}

PaymentRequestSheetController::~PaymentRequestSheetController() {}

std::unique_ptr<views::View> PaymentRequestSheetController::CreateView() {
  // Create the footer now so that it's known if there's a primary button or not
  // before creating the sheet view. This way, it's possible to determine
  // whether there's something to do when the user hits enter.
  std::unique_ptr<views::View> footer = CreateFooterView();
  auto view = std::make_unique<SheetView>(
      primary_button_
          ? base::Bind(
                &PaymentRequestSheetController::PerformPrimaryButtonAction,
                weak_ptr_factory_.GetWeakPtr())
          : base::Callback<void(bool*)>());

  DialogViewID sheet_id;
  if (GetSheetId(&sheet_id))
    view->SetID(static_cast<int>(sheet_id));

  view->SetBackground(views::CreateThemedSolidBackground(
      view.get(), ui::NativeTheme::kColorId_DialogBackground));

  // Paint the sheets to layers, otherwise the MD buttons (which do paint to a
  // layer) won't do proper clipping.
  view->SetPaintToLayer();

  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>());

  // Note: each view is responsible for its own padding (insets).
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  auto header_view = std::make_unique<views::View>();
  PopulateSheetHeaderView(
      ShouldShowHeaderBackArrow(), CreateHeaderContentView(header_view.get()),
      this, header_view.get(), GetHeaderBackground(header_view.get()));
  header_view_ = layout->AddView(std::move(header_view));

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  auto header_content_separator_container = std::make_unique<views::View>();
  header_content_separator_container_ =
      layout->AddView(std::move(header_content_separator_container));
  UpdateHeaderContentSeparatorView();

  layout->StartRow(1.0, 0);
  // |content_view| will go into a views::ScrollView so it needs to be sized now
  // otherwise it'll be sized to the ScrollView's viewport height, preventing
  // the scroll bar from ever being shown.
  auto pane = std::make_unique<views::View>();
  views::GridLayout* pane_layout =
      pane->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* pane_columns = pane_layout->AddColumnSet(0);
  pane_columns->AddColumn(
      views::GridLayout::Alignment::FILL, views::GridLayout::Alignment::LEADING,
      views::GridLayout::kFixedSize, views::GridLayout::SizeType::FIXED,
      GetActualDialogWidth(), GetActualDialogWidth());
  pane_layout->StartRow(views::GridLayout::kFixedSize, 0);
  // This is owned by its parent. It's the container passed to FillContentView.
  auto content_view = std::make_unique<views::View>();
  content_view->SetPaintToLayer();
  content_view->layer()->SetFillsBoundsOpaquely(true);
  content_view->SetBackground(views::CreateThemedSolidBackground(
      content_view.get(), ui::NativeTheme::kColorId_DialogBackground));
  content_view->SetID(static_cast<int>(DialogViewID::CONTENT_VIEW));
  content_view_ = pane_layout->AddView(std::move(content_view));
  pane->SizeToPreferredSize();

  std::unique_ptr<views::ScrollView> scroll =
      DisplayDynamicBorderForHiddenContents()
          ? std::make_unique<BorderedScrollView>()
          : std::make_unique<views::ScrollView>();
  scroll->SetHideHorizontalScrollBar(true);
  pane_ = scroll->SetContents(std::move(pane));
  scroll_ = layout->AddView(std::move(scroll));

  if (footer) {
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(std::move(footer));
  }

  UpdateContentView();

  view->SetFirstFocusableView(GetFirstFocusedView());
  return view;
}

void PaymentRequestSheetController::UpdateContentView() {
  content_view_->RemoveAllChildViews(true);
  FillContentView(content_view_);
  RelayoutPane();
}

void PaymentRequestSheetController::UpdateHeaderView() {
  header_view_->RemoveAllChildViews(true);
  PopulateSheetHeaderView(ShouldShowHeaderBackArrow(),
                          CreateHeaderContentView(header_view_), this,
                          header_view_, GetHeaderBackground(header_view_));
  header_view_->Layout();
  header_view_->SchedulePaint();
}

void PaymentRequestSheetController::UpdateHeaderContentSeparatorView() {
  header_content_separator_container_->RemoveAllChildViews(true);
  views::View* separator = CreateHeaderContentSeparatorView();
  if (separator) {
    header_content_separator_container_->SetLayoutManager(
        std::make_unique<views::FillLayout>());
    header_content_separator_container_->AddChildView(separator);
  }

  // Relayout sheet view after updating header content separator.
  DialogViewID sheet_id;
  if (!GetSheetId(&sheet_id))
    return;
  SheetView* sheet_view = static_cast<SheetView*>(
      dialog()->GetViewByID(static_cast<int>(sheet_id)));
  // This will be null on first call since it's not been set until CreateView
  // returns, and the first call to UpdateHeaderContentSeparatorView comes
  // from CreateView.
  if (sheet_view) {
    sheet_view->Layout();
  }
}

void PaymentRequestSheetController::UpdateFocus(views::View* focused_view) {
  DialogViewID sheet_id;
  if (GetSheetId(&sheet_id)) {
    SheetView* sheet_view = static_cast<SheetView*>(
        dialog()->GetViewByID(static_cast<int>(sheet_id)));
    // This will be null on first call since it's not been set until CreateView
    // returns, and the first call to UpdateContentView comes from CreateView.
    if (sheet_view) {
      sheet_view->SetFirstFocusableView(focused_view);
      dialog()->RequestFocus();
    }
  }
}

void PaymentRequestSheetController::RelayoutPane() {
  content_view_->Layout();
  pane_->SizeToPreferredSize();
  // Now that the content and its surrounding pane are updated, force a Layout
  // on the ScrollView so that it updates its scroll bars now.
  scroll_->Layout();
}

std::unique_ptr<views::Button>
PaymentRequestSheetController::CreatePrimaryButton() {
  return nullptr;
}

base::string16 PaymentRequestSheetController::GetSecondaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_CANCEL_PAYMENT);
}

int PaymentRequestSheetController::GetSecondaryButtonTag() {
  return static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG);
}

int PaymentRequestSheetController::GetSecondaryButtonId() {
  return static_cast<int>(DialogViewID::CANCEL_BUTTON);
}

bool PaymentRequestSheetController::ShouldShowSecondaryButton() {
  return true;
}

bool PaymentRequestSheetController::ShouldShowHeaderBackArrow() {
  return true;
}

std::unique_ptr<views::View>
PaymentRequestSheetController::CreateExtraFooterView() {
  return nullptr;
}

std::unique_ptr<views::View>
PaymentRequestSheetController::CreateHeaderContentView(
    views::View* header_view) {
  std::unique_ptr<views::Label> title_label = std::make_unique<views::Label>(
      GetSheetTitle(), views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetID(static_cast<int>(DialogViewID::SHEET_TITLE));
  title_label->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  return title_label;
}

views::View* PaymentRequestSheetController::CreateHeaderContentSeparatorView() {
  return nullptr;
}

std::unique_ptr<views::Background>
PaymentRequestSheetController::GetHeaderBackground(views::View* header_view) {
  return views::CreateThemedSolidBackground(
      header_view, ui::NativeTheme::kColorId_DialogBackground);
}

void PaymentRequestSheetController::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  if (!dialog()->IsInteractive())
    return;

  switch (static_cast<PaymentRequestCommonTags>(sender->tag())) {
    case PaymentRequestCommonTags::CLOSE_BUTTON_TAG:
      dialog()->CloseDialog();
      break;
    case PaymentRequestCommonTags::BACK_BUTTON_TAG:
      dialog()->GoBack();
      break;
    case PaymentRequestCommonTags::PAY_BUTTON_TAG:
      dialog()->Pay();
      break;
    case PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX:
      NOTREACHED();
      break;
  }
}

std::unique_ptr<views::View> PaymentRequestSheetController::CreateFooterView() {
  std::unique_ptr<views::View> container = std::make_unique<views::View>();

  // The distance between the elements and the dialog borders.
  constexpr int kInset = 16;
  container->SetBorder(
      views::CreateEmptyBorder(kInset, kInset, kInset, kInset));

  views::GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddPaddingColumn(1.0, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  std::unique_ptr<views::View> extra_view = CreateExtraFooterView();
  if (extra_view)
    layout->AddView(std::move(extra_view));
  else
    layout->SkipColumns(1);

  std::unique_ptr<views::View> trailing_buttons_container =
      std::make_unique<views::View>();

  trailing_buttons_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kPaymentRequestButtonSpacing));

#if defined(OS_MACOSX)
  AddSecondaryButton(trailing_buttons_container.get());
  AddPrimaryButton(trailing_buttons_container.get());
#else
  AddPrimaryButton(trailing_buttons_container.get());
  AddSecondaryButton(trailing_buttons_container.get());
#endif  // defined(OS_MACOSX)

  if (container->children().empty() &&
      trailing_buttons_container->children().empty()) {
    // If there's no extra view and no button, return null to signal that no
    // footer should be rendered.
    return nullptr;
  }

  layout->AddView(std::move(trailing_buttons_container));

  return container;
}

views::View* PaymentRequestSheetController::GetFirstFocusedView() {
  if (primary_button_ && primary_button_->GetEnabled())
    return primary_button_;

  if (secondary_button_)
    return secondary_button_;

  DCHECK(content_view_);
  return content_view_;
}

bool PaymentRequestSheetController::GetSheetId(DialogViewID* sheet_id) {
  return false;
}

bool PaymentRequestSheetController::DisplayDynamicBorderForHiddenContents() {
  return true;
}

void PaymentRequestSheetController::PerformPrimaryButtonAction(
    bool* is_enabled) {
  if (!dialog()->IsInteractive()) {
    // Set |is_enabled| to "true" to prevent other views from handling the
    // event.
    *is_enabled = true;
    return;
  }

  if (primary_button_ && primary_button_->GetEnabled())
    ButtonPressed(primary_button_, DummyEvent());

  *is_enabled = true;
}

void PaymentRequestSheetController::AddPrimaryButton(views::View* container) {
  std::unique_ptr<views::Button> primary_button = CreatePrimaryButton();
  if (primary_button) {
    primary_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    primary_button_ = container->AddChildView(std::move(primary_button));
  }
}

void PaymentRequestSheetController::AddSecondaryButton(views::View* container) {
  if (ShouldShowSecondaryButton()) {
    std::unique_ptr<views::Button> secondary_button =
        views::MdTextButton::CreateSecondaryUiButton(this,
                                                     GetSecondaryButtonLabel());
    secondary_button->set_tag(GetSecondaryButtonTag());
    secondary_button->SetID(GetSecondaryButtonId());
    secondary_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    secondary_button_ = container->AddChildView(std::move(secondary_button));
  }
}

}  // namespace payments
