// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/crostini_app_window.h"
#include "base/memory/raw_ptr.h"

#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "extensions/common/constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
constexpr int kWindowIconSizeDips = extension_misc::EXTENSION_ICON_BITTY;
constexpr int kAppIconSizeDips = extension_misc::EXTENSION_ICON_MEDIUM;
}  // namespace

// Handles an icon load and sets the appropriate widget icon based on the mode.
// Needed because the icon size provided to AppServiceAppIconLoader is a hint
// and the returned icon may be a different size. Therefore we can't use the
// size of the returned icon to decide whether it is the app or window icon.
class CrostiniAppWindow::IconLoader : public AppServiceAppIconLoader,
                                      public AppIconLoaderDelegate {
 public:
  enum class Mode { kAppIcon, kWindowIcon };
  IconLoader(Profile* profile, Mode mode, views::Widget* widget)
      : AppServiceAppIconLoader(
            profile,
            mode == Mode::kWindowIcon ? kWindowIconSizeDips : kAppIconSizeDips,
            this),
        mode_(mode),
        widget_(widget) {}
  IconLoader(const IconLoader&) = delete;
  IconLoader& operator=(const IconLoader&) = delete;
  ~IconLoader() override = default;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override {
    if (!widget_ || !widget_->widget_delegate())
      return;

    if (mode_ == Mode::kWindowIcon) {
      widget_->widget_delegate()->SetIcon(ui::ImageModel::FromImageSkia(image));
    } else {
      widget_->widget_delegate()->SetAppIcon(
          ui::ImageModel::FromImageSkia(image));
    }
  }

 private:
  const Mode mode_;
  const raw_ptr<views::Widget> widget_;
};

CrostiniAppWindow::CrostiniAppWindow(Profile* profile,
                                     const ash::ShelfID& shelf_id,
                                     views::Widget* widget)
    : AppWindowBase(shelf_id, widget) {
  app_icon_loader_ =
      std::make_unique<IconLoader>(profile, IconLoader::Mode::kAppIcon, widget);
  app_icon_loader_->FetchImage(shelf_id.app_id);
  window_icon_loader_ = std::make_unique<IconLoader>(
      profile, IconLoader::Mode::kWindowIcon, widget);
  window_icon_loader_->FetchImage(shelf_id.app_id);
}

CrostiniAppWindow::~CrostiniAppWindow() = default;
