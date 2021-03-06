// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_signin_window.h"

#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "chrome/browser/ui/webid/identity_dialogs.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/window/dialog_delegate.h"

#include "ui/views/layout/box_layout.h"

// Dimensions of the dialog itself.
constexpr int kDialogMinWidth = 512;
constexpr int kDialogHeight = 450;
// Dimension of the header.
constexpr int kHeaderHeight = 50;

// Creates the following UI:
// +----------------+
// |  Page Title    |
// |  URL           |
// +----------------+
class TitleAndOriginView : public views::View {
 public:
  METADATA_HEADER(TitleAndOriginView);
  TitleAndOriginView(const base::string16& page_title, const GURL& origin) {
    // The logic here is mostly based on Payments UI used for
    // `PaymentHandlerWebFlowViewController`.
    constexpr int kLeftPadding = 5;
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(0, kLeftPadding, 0, 0), 0 /* betweeen_child_spacing */));
    layout->set_minimum_cross_axis_size(kDialogMinWidth);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    bool title_is_valid = !page_title.empty();
    if (title_is_valid) {
      auto* title_label = AddChildView(std::make_unique<views::Label>(
          page_title, views::style::CONTEXT_DIALOG_TITLE));
      title_label->SetFocusBehavior(
          views::View::FocusBehavior::ACCESSIBLE_ONLY);
    }

    // We are not showing the schema since it is expected to always be
    // `https://`.
    CHECK(origin.SchemeIs(url::kHttpsScheme));
    auto* origin_label = AddChildView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(origin.host())));
    origin_label->SetElideBehavior(gfx::ELIDE_HEAD);
    if (!title_is_valid) {
      // Pad to keep header as the same height as when the page title is valid.
      constexpr int kVerticalPadding = 10;
      origin_label->SetBorder(
          views::CreateEmptyBorder(kVerticalPadding, 0, kVerticalPadding, 0));
    }
  }
  TitleAndOriginView(const TitleAndOriginView&) = delete;
  TitleAndOriginView& operator=(const TitleAndOriginView&) = delete;
  ~TitleAndOriginView() override = default;
};

BEGIN_METADATA(TitleAndOriginView, views::View)
END_METADATA

// The view for IDP sign in page.
// It observes the loaded web contents to update header information as the load
// progresses and in case it navigates.
class SigninDialogView : public views::BubbleDialogDelegateView,
                         public content::WebContentsObserver {
 public:
  METADATA_HEADER(SigninDialogView);
  SigninDialogView(content::WebContents* initiator_web_contents,
                   content::WebContents* idp_web_contents,
                   const GURL& provider)
      : initiator_web_contents_(initiator_web_contents), web_view_(nullptr) {
    // Create the following UI:
    // +----------------+
    // |   Header view  |
    // +--[separator]---+
    // |                |
    // |  Content View  |
    // |                |
    // +----------------+
    //
    // Currently the header view shows the title & URL using a
    // `TitleAndOriginView` and content view shows the IDP sign in page using a
    // `WebView`.
    DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
    SetModalType(ui::MODAL_TYPE_CHILD);
    SetShowCloseButton(true);
    set_margins(gfx::Insets());

    auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

    header_view_ = AddChildView(CreateHeaderView());
    auto* separator = AddChildView(std::make_unique<views::Separator>());
    separator->SetPreferredSize(
        {kDialogMinWidth, views::Separator::kThickness});

    web_view_ = AddChildView(CreateContentWebView(idp_web_contents, provider));
    // Observe the webiew to react to URL and title changes.
    Observe(web_view_->GetWebContents());
    // Update the header once after setting up the web view as it uses the URL
    // and title from the web view.
    UpdateHeaderView();

    SetInitiallyFocusedView(web_view_);
  }

  std::unique_ptr<views::WebView> CreateContentWebView(
      content::WebContents* idp_web_contents,
      const GURL& provider) {
    auto web_view = std::make_unique<views::WebView>(
        initiator_web_contents_->GetBrowserContext());

    web_view->SetWebContents(idp_web_contents);
    web_view->LoadInitialURL(provider);

    // The webview must get an explicitly set height otherwise the layout
    // doesn't make it fill its container. This is likely because it has no
    // content at the time of first layout (nothing has loaded yet). Because of
    // this, set it to. total_dialog_height - header_height. On the other hand,
    // the width will be properly set so it can be 0 here.
    web_view->SetPreferredSize(
        {kDialogMinWidth, kDialogHeight - kHeaderHeight});

    return web_view;
  }

  std::unique_ptr<views::View> CreateHeaderView() {
    auto header_view = std::make_unique<views::View>();
    header_view->SetLayoutManager(std::make_unique<views::FillLayout>());
    return header_view;
  }

  void UpdateHeaderView() {
    header_view_->RemoveAllChildViews(true);
    header_view_->AddChildView(std::make_unique<TitleAndOriginView>(
        web_view_->GetWebContents()->GetTitle(),
        web_view_->GetWebContents()->GetVisibleURL().GetOrigin()));
  }

  views::Widget* Show() {
    // ShowWebModalDialogViews takes ownership of this, by way of the
    // DeleteDelegate method.
    return constrained_window::ShowWebModalDialogViews(this,
                                                       initiator_web_contents_);
  }

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument())
      return;

    UpdateHeaderView();
  }

  void LoadProgressChanged(double progress) override {
    // Dialog view comes with a neat progressbar so we can use that to show the
    // progress.
    if (progress >= 1) {
      // hide the progress bar
      GetBubbleFrameView()->SetProgress(base::nullopt);
      return;
    }
    GetBubbleFrameView()->SetProgress(progress);
  }

  void TitleWasSet(content::NavigationEntry* entry) override {
    UpdateHeaderView();
  }

 private:
  content::WebContents* initiator_web_contents_;
  // The header of the dialog, owned by the view hierarchy.
  views::View* header_view_;
  // The contents of the dialog, owned by the view hierarchy.
  views::WebView* web_view_;
};

BEGIN_METADATA(SigninDialogView, views::BubbleDialogDelegateView)
END_METADATA

WebIdSigninWindow::WebIdSigninWindow(
    content::WebContents* initiator_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& provider,
    IdProviderWindowClosedCallback on_done) {
  // TODO(majidvp): What happens if we are handling multiple concurrent WebId
  // requests? At the moment we keep creating modal dialogs. This may be fine
  // when these requests belong to different tabs but may break down if they are
  // from the same tab or even share the same |initiator_web_contents| (e.g.,
  // two requests made from an iframe and its embedder frame). We need to
  // investigate this to ensure we are providing appropriate UX.
  // http://crbug.com/1141125
  auto* dialog =
      new SigninDialogView(initiator_web_contents, idp_web_contents, provider);

  // Set close callback to also call on_done. This ensures that if user closes
  // the IDP window the caller promise is rejected accordingly.
  dialog->SetCloseCallback(std::move(on_done));

  // SigninDialogView is a WidgetDelegate, owned by its views::Widget. It is
  // destroyed by `DeleteDelegate()` which is invoked by view hierarchy. Once
  // modal is deleted we should delete the window class as well.
  dialog->RegisterDeleteDelegateCallback(
      base::BindOnce([](WebIdSigninWindow* window) { delete window; },
                     base::Unretained(this)));

  dialog_ = dialog->Show();
}

void WebIdSigninWindow::Close() {
  dialog_->Close();
}

WebIdSigninWindow::~WebIdSigninWindow() = default;

WebIdSigninWindow* ShowWebIdSigninWindow(
    content::WebContents* initiator_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& provider,
    IdProviderWindowClosedCallback on_done) {
  return new WebIdSigninWindow(initiator_web_contents, idp_web_contents,
                               provider, std::move(on_done));
}

void CloseWebIdSigninWindow(WebIdSigninWindow* window) {
  window->Close();
}
