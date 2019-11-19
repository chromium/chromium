// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/chooser_bubble_ui.h"

#include "base/strings/string16.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "chrome/browser/ui/views/front_eliding_title_label.h"
#include "components/bubble/bubble_controller.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_client_view.h"

using bubble_anchor_util::AnchorConfiguration;

namespace {

AnchorConfiguration GetChooserAnchorConfiguration(Browser* browser) {
  return bubble_anchor_util::GetPageInfoAnchorConfiguration(browser);
}

gfx::Rect GetChooserAnchorRect(Browser* browser) {
  return bubble_anchor_util::GetPageInfoAnchorRect(browser);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// View implementation for the chooser bubble.
class ChooserBubbleUiViewDelegate : public views::BubbleDialogDelegateView,
                                    public views::TableViewObserver {
 public:
  ChooserBubbleUiViewDelegate(
      Browser* browser,
      std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserBubbleUiViewDelegate() override;

  // views::View:
  void AddedToWidget() override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // views::DialogDelegate:
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(Browser* browser);

  void set_bubble_reference(BubbleReference bubble_reference);
  void UpdateTableView() const;

 private:
  DeviceChooserContentView* device_chooser_content_view_;
  BubbleReference bubble_reference_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiViewDelegate);
};

ChooserBubbleUiViewDelegate::ChooserBubbleUiViewDelegate(
    Browser* browser,
    std::unique_ptr<ChooserController> chooser_controller)
    : device_chooser_content_view_(nullptr) {
  // ------------------------------------
  // | Chooser bubble title             |
  // | -------------------------------- |
  // | | option 0                     | |
  // | | option 1                     | |
  // | | option 2                     | |
  // | |                              | |
  // | |                              | |
  // | |                              | |
  // | -------------------------------- |
  // |           [ Connect ] [ Cancel ] |
  // |----------------------------------|
  // | Get help                         |
  // ------------------------------------

  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   chooser_controller->GetOkButtonLabel());
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   chooser_controller->GetCancelButtonLabel());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  device_chooser_content_view_ =
      new DeviceChooserContentView(this, std::move(chooser_controller));
  AddChildView(device_chooser_content_view_);

  DialogDelegate::SetExtraView(device_chooser_content_view_->CreateExtraView());

  UpdateAnchor(browser);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::CHOOSER_UI);
}

ChooserBubbleUiViewDelegate::~ChooserBubbleUiViewDelegate() {}

void ChooserBubbleUiViewDelegate::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      CreateFrontElidingTitleLabel(GetWindowTitle()));
}

base::string16 ChooserBubbleUiViewDelegate::GetWindowTitle() const {
  return device_chooser_content_view_->GetWindowTitle();
}

views::View* ChooserBubbleUiViewDelegate::GetInitiallyFocusedView() {
  const views::DialogClientView* dcv = GetDialogClientView();
  return dcv ? dcv->cancel_button() : nullptr;
}

bool ChooserBubbleUiViewDelegate::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return device_chooser_content_view_->IsDialogButtonEnabled(button);
}

bool ChooserBubbleUiViewDelegate::Accept() {
  device_chooser_content_view_->Accept();
  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
  return true;
}

bool ChooserBubbleUiViewDelegate::Cancel() {
  device_chooser_content_view_->Cancel();
  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_CANCELED);
  return true;
}

bool ChooserBubbleUiViewDelegate::Close() {
  device_chooser_content_view_->Close();
  return true;
}

void ChooserBubbleUiViewDelegate::OnSelectionChanged() {
  DialogModelChanged();
}

void ChooserBubbleUiViewDelegate::UpdateAnchor(Browser* browser) {
  AnchorConfiguration configuration = GetChooserAnchorConfiguration(browser);
  SetAnchorView(configuration.anchor_view);
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view)
    SetAnchorRect(GetChooserAnchorRect(browser));
  SetArrow(configuration.bubble_arrow);
}

void ChooserBubbleUiViewDelegate::set_bubble_reference(
    BubbleReference bubble_reference) {
  bubble_reference_ = bubble_reference;
}

void ChooserBubbleUiViewDelegate::UpdateTableView() const {
  device_chooser_content_view_->UpdateTableView();
}

//////////////////////////////////////////////////////////////////////////////
// ChooserBubbleUi
ChooserBubbleUi::ChooserBubbleUi(
    Browser* browser,
    std::unique_ptr<ChooserController> chooser_controller)
    : browser_(browser), chooser_bubble_ui_view_delegate_(nullptr) {
  DCHECK(browser_);
  DCHECK(chooser_controller);
  chooser_bubble_ui_view_delegate_ =
      new ChooserBubbleUiViewDelegate(browser, std::move(chooser_controller));
}

ChooserBubbleUi::~ChooserBubbleUi() {
  if (chooser_bubble_ui_view_delegate_ &&
      chooser_bubble_ui_view_delegate_->GetWidget()) {
    chooser_bubble_ui_view_delegate_->GetWidget()->RemoveObserver(this);
  }
}

void ChooserBubbleUi::Show(BubbleReference bubble_reference) {
  chooser_bubble_ui_view_delegate_->set_bubble_reference(bubble_reference);
  chooser_bubble_ui_view_delegate_->UpdateAnchor(browser_);
  CreateAndShow(chooser_bubble_ui_view_delegate_);
  chooser_bubble_ui_view_delegate_->GetWidget()->AddObserver(this);
  chooser_bubble_ui_view_delegate_->UpdateTableView();
}

void ChooserBubbleUi::Close() {
  if (chooser_bubble_ui_view_delegate_ &&
      !chooser_bubble_ui_view_delegate_->GetWidget()->IsClosed()) {
    chooser_bubble_ui_view_delegate_->GetWidget()->Close();
  }
}

void ChooserBubbleUi::UpdateAnchorPosition() {
  if (chooser_bubble_ui_view_delegate_)
    chooser_bubble_ui_view_delegate_->UpdateAnchor(browser_);
}

void ChooserBubbleUi::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  chooser_bubble_ui_view_delegate_ = nullptr;
}
