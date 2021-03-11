// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_signin_page_view.h"

#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/window/dialog_delegate.h"

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
  TitleAndOriginView(const std::u16string& page_title, const GURL& origin) {
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
SigninPageView::SigninPageView(WebIdDialogViews* dialog,
                               content::WebContents* initiator_web_contents,
                               content::WebContents* idp_web_contents,
                               const GURL& provider)
    : dialog_(dialog),
      initiator_web_contents_(initiator_web_contents),
      web_view_(nullptr) {
  // Create the following UI inside parent dialog:
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
  dialog_->SetButtons(ui::DIALOG_BUTTON_NONE);
  dialog_->SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);
  dialog_->SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, false);

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  header_view_ = AddChildView(CreateHeaderView());
  auto* separator = AddChildView(std::make_unique<views::Separator>());
  separator->SetPreferredSize({kDialogMinWidth, views::Separator::kThickness});

  web_view_ = AddChildView(CreateContentWebView(idp_web_contents, provider));
  // Observe the webiew to react to URL and title changes.
  Observe(web_view_->GetWebContents());
  // Update the header once after setting up the web view as it uses the URL
  // and title from the web view.
  UpdateHeaderView();
}

std::unique_ptr<views::WebView> SigninPageView::CreateContentWebView(
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
  web_view->SetPreferredSize({kDialogMinWidth, kDialogHeight - kHeaderHeight});

  return web_view;
}

std::unique_ptr<views::View> SigninPageView::CreateHeaderView() {
  auto header_view = std::make_unique<views::View>();
  header_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  return header_view;
}

void SigninPageView::UpdateHeaderView() {
  header_view_->RemoveAllChildViews(true);
  header_view_->AddChildView(std::make_unique<TitleAndOriginView>(
      web_view_->GetWebContents()->GetTitle(),
      web_view_->GetWebContents()->GetVisibleURL().GetOrigin()));
}

// content::WebContentsObserver:
void SigninPageView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  UpdateHeaderView();
}

void SigninPageView::LoadProgressChanged(double progress) {
  // Dialog view comes with a neat progressbar so we can use that to show the
  // progress.
  if (progress >= 1) {
    // hide the progress bar
    dialog_->GetBubbleFrameView()->SetProgress(base::nullopt);
    return;
  }
  dialog_->GetBubbleFrameView()->SetProgress(progress);
}

void SigninPageView::TitleWasSet(content::NavigationEntry* entry) {
  UpdateHeaderView();
}

BEGIN_METADATA(SigninPageView, views::View)
END_METADATA
