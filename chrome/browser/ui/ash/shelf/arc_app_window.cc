// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/arc_app_window.h"

#include "ash/components/arc/arc_util.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/arc_app_window_delegate.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr base::TimeDelta kSetDefaultIconDelayMs = base::Milliseconds(1000);

constexpr int kArcAppWindowIconSize = extension_misc::EXTENSION_ICON_MEDIUM;
}  // namespace

ArcAppWindow::ArcAppWindow(const arc::ArcAppShelfId& app_shelf_id,
                           views::Widget* widget,
                           ArcAppWindowDelegate* owner,
                           Profile* profile)
    : AppWindowBase(ash::ShelfID(app_shelf_id.app_id()), widget),
      app_shelf_id_(app_shelf_id),
      owner_(owner),
      profile_(profile) {
  DCHECK(owner_);

  // AppService uses app_shelf_id as the app_id to construct ShelfID.
  set_shelf_id(ash::ShelfID(app_shelf_id.ToString()));

  SetDefaultAppIcon();
}

ArcAppWindow::~ArcAppWindow() = default;

void ArcAppWindow::SetFullscreenMode(FullScreenMode mode) {
  DCHECK(mode != FullScreenMode::kNotDefined);
  fullscreen_mode_ = mode;
}

void ArcAppWindow::SetDescription(const std::string& title,
                                  const gfx::ImageSkia& icon) {
  if (!title.empty())
    GetNativeWindow()->SetTitle(base::UTF8ToUTF16(title));
  if (icon.isNull()) {
    // Reset custom icon. Switch back to default.
    SetDefaultAppIcon();
    return;
  }

  app_icon_loader_.reset();
  if (kArcAppWindowIconSize > icon.width() ||
      kArcAppWindowIconSize > icon.height()) {
    LOG(WARNING) << "An icon of size " << icon.width() << "x" << icon.height()
                 << " is being scaled up and will look blurry.";
  }
  SetIcon(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kArcAppWindowIconSize, kArcAppWindowIconSize)));
}

bool ArcAppWindow::IsActive() const {
  return widget()->IsActive() &&
         (owner_->GetActiveTaskId() ==
              arc::GetWindowTaskId(GetNativeWindow()) ||
          owner_->GetActiveSessionId() ==
              arc::GetWindowSessionId(GetNativeWindow()));
}

void ArcAppWindow::Close() {
  auto task_id = arc::GetWindowTaskId(GetNativeWindow());
  if (task_id.has_value())
    arc::CloseTask(*task_id);
}

void ArcAppWindow::OnAppImageUpdated(
    const std::string& app_id,
    const gfx::ImageSkia& image,
    bool is_placeholder_icon,
    const std::optional<gfx::ImageSkia>& badge_image) {
  if (image_fetching_) {
    // This is default app icon. Don't assign it right now to avoid flickering.
    // Wait for another image is loaded and only in case next image is not
    // coming set this as a fallback.
    apply_default_image_timer_.Start(
        FROM_HERE, kSetDefaultIconDelayMs,
        base::BindOnce(&ArcAppWindow::SetIcon, base::Unretained(this), image));
  } else {
    SetIcon(image);
  }
}

void ArcAppWindow::SetDefaultAppIcon() {
  if (!app_icon_loader_) {
    app_icon_loader_ = std::make_unique<AppServiceAppIconLoader>(
        profile_, kArcAppWindowIconSize, this);
  }
  DCHECK(!image_fetching_);
  base::AutoReset<bool> auto_image_fetching(&image_fetching_, true);
  app_icon_loader_->FetchImage(app_shelf_id_.ToString());
}

void ArcAppWindow::SetIcon(const gfx::ImageSkia& icon) {
  // Reset any pending request to set default app icon.
  apply_default_image_timer_.Stop();

  if (!exo::GetShellRootSurface(GetNativeWindow())) {
    // Support unit tests where we don't have exo system initialized.
    views::NativeWidgetAura::AssignIconToAuraWindow(
        GetNativeWindow(), gfx::ImageSkia() /* window_icon */,
        icon /* app_icon */);
    return;
  }
  exo::ShellSurfaceBase* shell_surface = static_cast<exo::ShellSurfaceBase*>(
      widget()->widget_delegate()->GetContentsView());
  if (!shell_surface)
    return;
  shell_surface->SetIcon(icon);
}
