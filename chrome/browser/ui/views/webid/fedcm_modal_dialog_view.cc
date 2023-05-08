// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"

#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"

FedCmModalDialogView::FedCmModalDialogView(
    content::WebContents* web_contents,
    const GURL& url,
    FedCmModalDialogView::Observer* observer)
    : web_contents_(web_contents),
      observer_(observer),
      curr_origin_(url::Origin::Create(url)) {
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  Init(url);
}

FedCmModalDialogView::~FedCmModalDialogView() {
  // Let the observer know that this object is being destroyed.
  if (observer_) {
    observer_->OnFedCmModalDialogViewDestroyed();
  }
}

// static
FedCmModalDialogView* FedCmModalDialogView::ShowFedCmModalDialog(
    content::WebContents* web_contents,
    const GURL& url,
    FedCmModalDialogView::Observer* observer) {
  // This dialog owns itself. DialogDelegateView will delete |dialog| instance.
  FedCmModalDialogView* dialog =
      new FedCmModalDialogView(web_contents, url, observer);
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
  return dialog;
}

void FedCmModalDialogView::CloseFedCmModalDialog() {
  contents_wrapper_->GetWidget()->Close();
}

content::WebContents* FedCmModalDialogView::GetWebViewWebContents() {
  DCHECK(web_view_);
  return web_view_->GetWebContents();
}

void FedCmModalDialogView::RemoveObserver() {
  observer_ = nullptr;
}

void FedCmModalDialogView::Init(const GURL& url) {
  constexpr int kDialogMinWidth = 512;
  constexpr int kDialogHeight = 450;
  constexpr int kVerticalInset = 8;
  constexpr int kHeaderHorizontalInset = 16;

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto contents_wrapper = std::make_unique<views::View>();
  contents_wrapper->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  contents_wrapper->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalInset, kHeaderHorizontalInset, kVerticalInset,
                        kHeaderHorizontalInset)));

  auto* header_view =
      contents_wrapper->AddChildView(std::make_unique<views::View>());
  header_view = PopulateSheetHeaderView(header_view, url);

  web_view_ = contents_wrapper->AddChildView(
      std::make_unique<views::WebView>(web_contents_->GetBrowserContext()));
  web_view_->SetPreferredSize(gfx::Size(kDialogMinWidth, kDialogHeight));
  web_view_->LoadInitialURL(url);

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_view_->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      web_view_->GetWebContents())
      ->SetDelegate(this);

  Observe(web_view_->GetWebContents());

  contents_wrapper_ = AddChildView(std::move(contents_wrapper));
}

views::View* FedCmModalDialogView::PopulateSheetHeaderView(
    views::View* container,
    const GURL& url) {
  views::TableLayout* layout =
      container->SetLayoutManager(std::make_unique<views::TableLayout>());
  layout->AddRows(1, views::TableLayout::kFixedSize);

  // Origin column.
  layout->AddColumn(
      views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
      /*horizontal_resize=*/1.0, views::TableLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0,
      /*min_width=*/0);
  layout->AddRows(1, views::TableLayout::kFixedSize);

  // Add the origin label.
  origin_label_ = container->AddChildView(std::make_unique<views::Label>(
      url_formatter::FormatOriginForSecurityDisplay(
          curr_origin_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)));
  origin_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  origin_label_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  return container;
}

void FedCmModalDialogView::PrimaryPageChanged(content::Page& page) {
  const url::Origin origin = page.GetMainDocument().GetLastCommittedOrigin();
  if (!origin_label_ || origin.IsSameOriginWith(curr_origin_)) {
    return;
  }

  // Update the origin label.
  origin_label_->SetText(url_formatter::FormatOriginForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  curr_origin_ = origin;
}

BEGIN_METADATA(FedCmModalDialogView, views::DialogDelegateView)
END_METADATA
