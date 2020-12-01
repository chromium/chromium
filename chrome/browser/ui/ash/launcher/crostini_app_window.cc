// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/crostini_app_window.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_icon_loader.h"
#include "extensions/common/constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
constexpr int kIconSize = extension_misc::EXTENSION_ICON_MEDIUM;
}  // namespace

CrostiniAppWindow::CrostiniAppWindow(Profile* profile,
                                     const ash::ShelfID& shelf_id,
                                     views::Widget* widget)
    : AppWindowBase(shelf_id, widget) {
  app_icon_loader_ =
      std::make_unique<AppServiceAppIconLoader>(profile, kIconSize, this);
  app_icon_loader_->FetchImage(shelf_id.app_id);
}

CrostiniAppWindow::~CrostiniAppWindow() = default;

void CrostiniAppWindow::OnAppImageUpdated(const std::string& app_id,
                                          const gfx::ImageSkia& image) {
  if (!widget() || !widget()->widget_delegate())
    return;

  widget()->widget_delegate()->SetIcon(image);
}
