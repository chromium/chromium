// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_progress_view.h"

#include <memory>
#include <utility>

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace web_app {

WebAppInstallProgressView::WebAppInstallProgressView() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(10);

  progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar_->SetValue(0.0);
}

WebAppInstallProgressView::~WebAppInstallProgressView() = default;

void WebAppInstallProgressView::SetProgressValue(double progress) {
  progress_bar_->SetValue(progress);
}

BEGIN_METADATA(WebAppInstallProgressView)
END_METADATA

}  // namespace web_app
