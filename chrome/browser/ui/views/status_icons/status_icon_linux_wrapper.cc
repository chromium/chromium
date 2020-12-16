// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/status_icons/status_icon_button_linux.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if defined(USE_DBUS)
#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"
#endif

namespace {

gfx::ImageSkia GetBestImageRep(const gfx::ImageSkia& image) {
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
  return gfx::ImageSkia(gfx::ImageSkiaRep(best_rep, 1.0f));
}

}  // namespace

StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    views::StatusIconLinux* status_icon,
    StatusIconType status_icon_type,
    const gfx::ImageSkia& image,
    const base::string16& tool_tip)
    : status_icon_(status_icon),
      status_icon_type_(status_icon_type),
      image_(GetBestImageRep(image)),
      tool_tip_(tool_tip),
      menu_model_(nullptr) {
  status_icon_->SetDelegate(this);
}

#if defined(USE_DBUS)
StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    scoped_refptr<StatusIconLinuxDbus> status_icon,
    const gfx::ImageSkia& image,
    const base::string16& tool_tip)
    : StatusIconLinuxWrapper(status_icon.get(), kTypeDbus, image, tool_tip) {
  status_icon_dbus_ = status_icon;
}
#endif

StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    std::unique_ptr<views::StatusIconLinux> status_icon,
    StatusIconType status_icon_type,
    const gfx::ImageSkia& image,
    const base::string16& tool_tip)
    : StatusIconLinuxWrapper(status_icon.get(),
                             status_icon_type,
                             image,
                             tool_tip) {
  status_icon_linux_ = std::move(status_icon);
}

StatusIconLinuxWrapper::~StatusIconLinuxWrapper() {
  if (menu_model_)
    menu_model_->RemoveObserver(this);
}

void StatusIconLinuxWrapper::SetImage(const gfx::ImageSkia& image) {
  image_ = GetBestImageRep(image);
  if (status_icon_)
    status_icon_->SetIcon(image_);
}

void StatusIconLinuxWrapper::SetToolTip(const base::string16& tool_tip) {
  tool_tip_ = tool_tip;
  if (status_icon_)
    status_icon_->SetToolTip(tool_tip);
}

void StatusIconLinuxWrapper::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const base::string16& title,
    const base::string16& contents,
    const message_center::NotifierId& notifier_id) {
  notification_.DisplayBalloon(icon, title, contents, notifier_id);
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

const base::string16& StatusIconLinuxWrapper::GetToolTip() const {
  return tool_tip_;
}

ui::MenuModel* StatusIconLinuxWrapper::GetMenuModel() const {
  return menu_model_;
}

void StatusIconLinuxWrapper::OnImplInitializationFailed() {
  switch (status_icon_type_) {
    case kTypeDbus:
#if defined(USE_DBUS)
      status_icon_dbus_.reset();
#endif
      status_icon_linux_ = std::make_unique<StatusIconButtonLinux>();
      status_icon_ = status_icon_linux_.get();
      status_icon_type_ = kTypeWindowed;
      status_icon_->SetDelegate(this);
      return;
    case kTypeWindowed:
      status_icon_linux_.reset();
      status_icon_ = nullptr;
      status_icon_type_ = kTypeNone;
      if (menu_model_)
        menu_model_->RemoveObserver(this);
      menu_model_ = nullptr;
      return;
    case kTypeNone:
      NOTREACHED();
  }
}

void StatusIconLinuxWrapper::OnMenuStateChanged() {
  if (status_icon_)
    status_icon_->RefreshPlatformContextMenu();
}

std::unique_ptr<StatusIconLinuxWrapper>
StatusIconLinuxWrapper::CreateWrappedStatusIcon(
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) {
#if defined(USE_DBUS)
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
  if (!status_icon_)
    return;

  // If a menu already exists, remove ourself from its observer list.
  if (menu_model_)
    menu_model_->RemoveObserver(this);

  status_icon_->UpdatePlatformContextMenu(model);
  menu_model_ = model;

  if (model)
    model->AddObserver(this);
}
