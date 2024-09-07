// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "components/permissions/chooser_controller.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension.h"
#endif

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
  METADATA_HEADER(ChooserBubbleUiViewDelegate, LocationBarBubbleDelegateView)

 public:
  ChooserBubbleUiViewDelegate(
      Browser* browser,
      content::WebContents* web_contents,
      std::unique_ptr<permissions::ChooserController> chooser_controller);

  ChooserBubbleUiViewDelegate(const ChooserBubbleUiViewDelegate&) = delete;
  ChooserBubbleUiViewDelegate& operator=(const ChooserBubbleUiViewDelegate&) =
      delete;

  ~ChooserBubbleUiViewDelegate() override;

  // views::View:
  void AddedToWidget() override;

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;

  // views::DialogDelegate:
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(Browser* browser);

  void UpdateTableView() const;

  base::OnceClosure MakeCloseClosure();
  void Close();

 private:
  raw_ptr<DeviceChooserContentView> device_chooser_content_view_ = nullptr;

  base::WeakPtrFactory<ChooserBubbleUiViewDelegate> weak_ptr_factory_{this};
};

ChooserBubbleUiViewDelegate::ChooserBubbleUiViewDelegate(
    Browser* browser,
    content::WebContents* contents,
    std::unique_ptr<permissions::ChooserController> chooser_controller)
    : LocationBarBubbleDelegateView(
          GetChooserAnchorConfiguration(browser).anchor_view,
          contents) {
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

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 chooser_controller->GetOkButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 chooser_controller->GetCancelButtonLabel());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  device_chooser_content_view_ =
      new DeviceChooserContentView(this, std::move(chooser_controller));
  AddChildView(device_chooser_content_view_.get());

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
}

ChooserBubbleUiViewDelegate::~ChooserBubbleUiViewDelegate() = default;

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
    ui::mojom::DialogButton button) const {
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

BEGIN_METADATA(ChooserBubbleUiViewDelegate)
END_METADATA

namespace chrome {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
base::OnceClosure ShowDeviceChooserDialogForExtension(
    content::RenderFrameHost* owner,
    const extensions::Extension* extension,
    std::unique_ptr<permissions::ChooserController> controller) {
  auto* contents = content::WebContents::FromRenderFrameHost(owner);
  auto* browser = chrome::FindBrowserWithTab(contents);
  if (!browser)
    return base::DoNothing();

  if (browser->tab_strip_model()->GetActiveWebContents() != contents)
    return base::DoNothing();

  // `GetExtensionsToolbarContainer` may return `nullptr`, for instance in
  // extension popup windows.
  auto* extensions_toolbar_container =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetExtensionsToolbarContainer();
  if (!extensions_toolbar_container) {
    return base::DoNothing();
  }

  auto bubble = std::make_unique<ChooserBubbleUiViewDelegate>(
      browser, contents, std::move(controller));
  base::OnceClosure close_closure = bubble->MakeCloseClosure();
  extensions_toolbar_container->ShowWidgetForExtension(
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble)),
      extension->id());
  return close_closure;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

base::OnceClosure ShowDeviceChooserDialog(
    content::RenderFrameHost* owner,
    std::unique_ptr<permissions::ChooserController> controller) {
  auto* contents = content::WebContents::FromRenderFrameHost(owner);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* browser_context = owner->GetBrowserContext();
  if (extensions::AppWindowRegistry::Get(browser_context)
          ->GetAppWindowForWebContents(contents)) {
    extensions::ShowConstrainedDeviceChooserDialog(contents,
                                                   std::move(controller));
    // This version of the chooser dialog does not support being closed by the
    // code which created it.
    return base::DoNothing();
  }

  auto origin = owner->GetMainFrame()->GetLastCommittedOrigin();
  if (origin.scheme() == extensions::kExtensionScheme) {
    const auto* extension =
        extensions::ExtensionRegistry::Get(browser_context)
            ->GetExtensionById(origin.host(),
                               extensions::ExtensionRegistry::EVERYTHING);
    DCHECK(extension);
    return ShowDeviceChooserDialogForExtension(owner, extension,
                                               std::move(controller));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  auto* browser = chrome::FindBrowserWithTab(contents);
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

  // If we're opening this device chooser dialog on a picture-in-picture window,
  // then our widget is also always-on-top and needs to be tracked by the
  // PictureInPictureOcclusionTracker so it can handle our widget occluding
  // other widgets.
  if (browser->is_type_picture_in_picture()) {
    PictureInPictureOcclusionTracker* tracker =
        PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
    if (tracker) {
      tracker->OnPictureInPictureWidgetOpened(widget);
    }
  }

  return close_closure;
}

}  // namespace chrome
