// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"

#include <memory>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/status_icons/status_icon_button_linux.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(USE_DBUS)
#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"
#endif

namespace {

gfx::ImageSkia GetBestImageRep(const gfx::ImageSkia& image) {
  image.EnsureRepsForSupportedScales();
  float best_scale = 0.0f;
  SkBitmap best_rep;
  for (const auto& rep : image.image_reps()) {
    if (rep.scale() > best_scale) {
      best_scale = rep.scale();
      best_rep = rep.GetBitmap();
    }
  }
  // All status icon implementations want the image in pixel coordinates, so use
  // a scale factor of 1.
  return gfx::ImageSkia::CreateFromBitmap(best_rep, 1.0f);
}

}  // namespace

StatusIconLinuxWrapper::StatusIconLinuxWrapper(ui::StatusIconLinux* status_icon,
                                               StatusIconType status_icon_type,
                                               const gfx::ImageSkia& image,
                                               const std::u16string& tool_tip)
    : status_icon_type_(status_icon_type),
      image_(GetBestImageRep(image)),
      tool_tip_(tool_tip),
      menu_model_(nullptr) {
  status_icon->SetDelegate(this);
}

#if BUILDFLAG(USE_DBUS)
StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    scoped_refptr<StatusIconLinuxDbus> status_icon,
    const gfx::ImageSkia& image,
    const std::u16string& tool_tip)
    : StatusIconLinuxWrapper(status_icon.get(), kTypeDbus, image, tool_tip) {
  status_icon_dbus_ = status_icon;
}
#endif

StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    std::unique_ptr<ui::StatusIconLinux> status_icon,
    StatusIconType status_icon_type,
    const gfx::ImageSkia& image,
    const std::u16string& tool_tip)
    : StatusIconLinuxWrapper(status_icon.get(),
                             status_icon_type,
                             image,
                             tool_tip) {
  status_icon_linux_ = std::move(status_icon);
}

StatusIconLinuxWrapper::~StatusIconLinuxWrapper() {
  if (menu_model_) {
    menu_model_->RemoveObserver(this);
  }

  if (auto* status_icon = GetStatusIcon()) {
    status_icon->SetDelegate(nullptr);
  }
#if BUILDFLAG(USE_DBUS)
  // The D-Bus icon may still be waiting for a StatusNotifierWatcher.
  if (status_icon_dbus_) {
    status_icon_dbus_->SetDelegate(nullptr);
  }
#endif
}

void StatusIconLinuxWrapper::SetImage(const gfx::ImageSkia& image) {
  image_ = GetBestImageRep(image);
  if (auto* status_icon = GetStatusIcon()) {
    status_icon->SetImage(image_);
  }
}

void StatusIconLinuxWrapper::SetIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;
  if (auto* status_icon = GetStatusIcon()) {
    status_icon->SetIcon(*icon_);
  }
}

void StatusIconLinuxWrapper::SetToolTip(const std::u16string& tool_tip) {
  tool_tip_ = tool_tip;
  if (auto* status_icon = GetStatusIcon()) {
    status_icon->SetToolTip(tool_tip);
  }
}

void StatusIconLinuxWrapper::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const std::u16string& title,
    const std::u16string& contents,
    const message_center::NotifierId& notifier_id) {
  notification_.DisplayBalloon(ui::ImageModel::FromImageSkia(icon), title,
                               contents, notifier_id);
}

void StatusIconLinuxWrapper::OnClick() {
  DispatchClickEvent();
}

bool StatusIconLinuxWrapper::HasClickAction() {
  return HasObservers();
}

const gfx::ImageSkia& StatusIconLinuxWrapper::GetImage() const {
  return image_;
}

const gfx::VectorIcon* StatusIconLinuxWrapper::GetIcon() const {
  return icon_;
}

const std::u16string& StatusIconLinuxWrapper::GetToolTip() const {
  return tool_tip_;
}

ui::MenuModel* StatusIconLinuxWrapper::GetMenuModel() const {
  return menu_model_;
}

void StatusIconLinuxWrapper::OnImplInitializationFailed() {
  switch (status_icon_type_) {
#if BUILDFLAG(USE_DBUS)
    case kTypeDbus:
      status_icon_dbus_.reset();
      status_icon_linux_ = std::make_unique<StatusIconButtonLinux>();
      status_icon_type_ = kTypeWindowed;
      status_icon_linux_->SetDelegate(this);
      return;
#endif
    case kTypeWindowed:
      status_icon_linux_.reset();
      status_icon_type_ = kTypeNone;
#if BUILDFLAG(USE_DBUS)
      if (status_icon_dbus_) {
        // Keep the menu for the D-Bus icon, which may still initialize.
        return;
      }
#endif
      if (menu_model_) {
        menu_model_->RemoveObserver(this);
      }
      menu_model_ = nullptr;
      return;
    case kTypeNone:
      NOTREACHED();
  }
}

void StatusIconLinuxWrapper::OnImplInitializationDelayed() {
#if BUILDFLAG(USE_DBUS)
  // Only the D-Bus icon delays initialization. Keep it, and use the windowed
  // icon until it initializes.
  CHECK_EQ(status_icon_type_, kTypeDbus);
  status_icon_linux_ = std::make_unique<StatusIconButtonLinux>();
  status_icon_type_ = kTypeWindowed;
  status_icon_linux_->SetDelegate(this);
#else
  NOTREACHED();
#endif
}

void StatusIconLinuxWrapper::OnImplInitialized() {
#if BUILDFLAG(USE_DBUS)
  if (status_icon_type_ == kTypeDbus || !status_icon_dbus_) {
    return;
  }

  // The D-Bus icon initialized after a delay. Replace the fallback with it, and
  // update it with any changes made since it read its initial state.
  status_icon_linux_.reset();
  status_icon_type_ = kTypeDbus;
  status_icon_dbus_->UpdatePlatformContextMenu(menu_model_);
  status_icon_dbus_->SetToolTip(tool_tip_);
  if (icon_) {
    status_icon_dbus_->SetIcon(*icon_);
  } else {
    status_icon_dbus_->SetImage(image_);
  }
#endif
}

void StatusIconLinuxWrapper::OnMenuStateChanged() {
  if (auto* status_icon = GetStatusIcon()) {
    status_icon->RefreshPlatformContextMenu();
  }
}

std::unique_ptr<StatusIconLinuxWrapper>
StatusIconLinuxWrapper::CreateWrappedStatusIcon(
    const gfx::ImageSkia& image,
    const std::u16string& tool_tip) {
#if BUILDFLAG(USE_DBUS)
  return base::WrapUnique(new StatusIconLinuxWrapper(
      base::MakeRefCounted<StatusIconLinuxDbus>(), image, tool_tip));
#else
  return base::WrapUnique(
      new StatusIconLinuxWrapper(std::make_unique<StatusIconButtonLinux>(),
                                 kTypeWindowed, image, tool_tip));
#endif
}

void StatusIconLinuxWrapper::UpdatePlatformContextMenu(
    StatusIconMenuModel* model) {
  bool has_status_icon = GetStatusIcon() != nullptr;
#if BUILDFLAG(USE_DBUS)
  // Keep the menu for a D-Bus icon that may still initialize.
  has_status_icon = has_status_icon || status_icon_dbus_;
#endif
  if (!has_status_icon) {
    return;
  }

  // If a menu already exists, remove ourself from its observer list.
  if (menu_model_) {
    menu_model_->RemoveObserver(this);
  }

  if (auto* status_icon = GetStatusIcon()) {
    status_icon->UpdatePlatformContextMenu(model);
  }
  menu_model_ = model;

  if (model) {
    model->AddObserver(this);
  }
}

ui::StatusIconLinux* StatusIconLinuxWrapper::GetStatusIcon() {
  switch (status_icon_type_) {
#if BUILDFLAG(USE_DBUS)
    case kTypeDbus:
      return status_icon_dbus_.get();
#endif
    case kTypeWindowed:
      return status_icon_linux_.get();
    case kTypeNone:
      return nullptr;
  }
}
