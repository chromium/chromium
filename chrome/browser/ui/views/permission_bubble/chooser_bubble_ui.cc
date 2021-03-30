// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

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
class ChooserBubbleUiViewDelegate : public LocationBarBubbleDelegateView,
                                    public views::TableViewObserver {
 public:
  ChooserBubbleUiViewDelegate(
      Browser* browser,
      content::WebContents* web_contents,
      std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserBubbleUiViewDelegate() override;

  // views::View:
  void AddedToWidget() override;

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;

  // views::DialogDelegate:
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(Browser* browser);

  void UpdateTableView() const;

  base::OnceClosure MakeCloseClosure();
  void Close();

  static int g_num_instances_for_testing_;

 private:
  DeviceChooserContentView* device_chooser_content_view_ = nullptr;

  base::WeakPtrFactory<ChooserBubbleUiViewDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiViewDelegate);
};

int ChooserBubbleUiViewDelegate::g_num_instances_for_testing_ = 0;

ChooserBubbleUiViewDelegate::ChooserBubbleUiViewDelegate(
    Browser* browser,
    content::WebContents* contents,
    std::unique_ptr<ChooserController> chooser_controller)
    : LocationBarBubbleDelegateView(
          GetChooserAnchorConfiguration(browser).anchor_view,
          contents) {
  g_num_instances_for_testing_++;
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

  SetButtonLabel(ui::DIALOG_BUTTON_OK, chooser_controller->GetOkButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 chooser_controller->GetCancelButtonLabel());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  device_chooser_content_view_ =
      new DeviceChooserContentView(this, std::move(chooser_controller));
  AddChildView(device_chooser_content_view_);

  SetExtraView(device_chooser_content_view_->CreateExtraView());

  SetAcceptCallback(
      base::BindOnce(&DeviceChooserContentView::Accept,
                     base::Unretained(device_chooser_content_view_)));
  SetCancelCallback(
      base::BindOnce(&DeviceChooserContentView::Cancel,
                     base::Unretained(device_chooser_content_view_)));
  SetCloseCallback(
      base::BindOnce(&DeviceChooserContentView::Close,
                     base::Unretained(device_chooser_content_view_)));

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CHOOSER_UI);
}

ChooserBubbleUiViewDelegate::~ChooserBubbleUiViewDelegate() {
  g_num_instances_for_testing_--;
}

void ChooserBubbleUiViewDelegate::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(CreateTitleOriginLabel(GetWindowTitle()));
}

std::u16string ChooserBubbleUiViewDelegate::GetWindowTitle() const {
  return device_chooser_content_view_->GetWindowTitle();
}

views::View* ChooserBubbleUiViewDelegate::GetInitiallyFocusedView() {
  return GetCancelButton();
}

bool ChooserBubbleUiViewDelegate::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return device_chooser_content_view_->IsDialogButtonEnabled(button);
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

void ChooserBubbleUiViewDelegate::UpdateTableView() const {
  device_chooser_content_view_->UpdateTableView();
}

base::OnceClosure ChooserBubbleUiViewDelegate::MakeCloseClosure() {
  return base::BindOnce(&ChooserBubbleUiViewDelegate::Close,
                        weak_ptr_factory_.GetWeakPtr());
}

void ChooserBubbleUiViewDelegate::Close() {
  if (GetWidget())
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

namespace chrome {

base::OnceClosure ShowDeviceChooserDialog(
    content::RenderFrameHost* owner,
    std::unique_ptr<ChooserController> controller) {
  auto* contents = content::WebContents::FromRenderFrameHost(owner);
  auto* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return base::DoNothing();

  if (browser->tab_strip_model()->GetActiveWebContents() != contents)
    return base::DoNothing();

  auto bubble = std::make_unique<ChooserBubbleUiViewDelegate>(
      browser, contents, std::move(controller));

  // Set |parent_window_| because some valid anchors can become hidden.
  views::Widget* parent_widget = views::Widget::GetWidgetForNativeWindow(
      browser->window()->GetNativeWindow());
  gfx::NativeView parent = parent_widget->GetNativeView();
  DCHECK(parent);
  bubble->set_parent_window(parent);
  bubble->UpdateAnchor(browser);

  base::OnceClosure close_closure = bubble->MakeCloseClosure();
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  if (browser->window()->IsActive())
    widget->Show();
  else
    widget->ShowInactive();

  return close_closure;
}

bool IsDeviceChooserShowingForTesting(Browser* browser) {
  return ChooserBubbleUiViewDelegate::g_num_instances_for_testing_ > 0;
}

}  // namespace chrome
