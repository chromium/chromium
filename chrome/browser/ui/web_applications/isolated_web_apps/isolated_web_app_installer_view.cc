// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/isolated_web_apps/isolated_web_app_installer_view.h"

#include "chrome/browser/ui/web_applications/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace web_app {

namespace {

std::unique_ptr<views::Label> CreateLabel(int text_id) {
  return std::make_unique<views::Label>(l10n_util::GetStringUTF16(text_id));
}

class GetMetadataView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(GetMetadataView);

  GetMetadataView() {
    views::LayoutProvider* provider = views::LayoutProvider::Get();
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetBetweenChildSpacing(
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));

    // TODO(crbug.com/1479140): Use the correct string here
    AddChildView(CreateLabel(IDS_PROMISE_STATUS_INSTALLING));
  }
};
BEGIN_METADATA(GetMetadataView, views::BoxLayoutView)
END_METADATA

}  // namespace

IsolatedWebAppInstallerView::IsolatedWebAppInstallerView(Delegate* delegate)
    : delegate_(delegate), active_view_(nullptr) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetInsideBorderInsets(
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG));
}

IsolatedWebAppInstallerView::~IsolatedWebAppInstallerView() = default;

void IsolatedWebAppInstallerView::ShowDisabledScreen() {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::ShowGetMetadataScreen() {
  SetActiveView(std::make_unique<GetMetadataView>());
}

void IsolatedWebAppInstallerView::UpdateGetMetadataProgress(double percent) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::ShowMetadataScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::ShowInstallScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::UpdateInstallProgress(double percent) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::ShowInstallSuccessScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::SetActiveView(std::unique_ptr<View> view) {
  if (active_view_) {
    RemoveChildView(active_view_);
  }
  active_view_ = AddChildView(std::move(view));
  InvalidateLayout();
}

BEGIN_METADATA(IsolatedWebAppInstallerView, views::BoxLayoutView)
END_METADATA

}  // namespace web_app
