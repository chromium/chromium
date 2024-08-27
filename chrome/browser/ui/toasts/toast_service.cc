// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/simple_menu_model.h"

ToastService::ToastService(BrowserWindowInterface* browser_window_interface) {
  toast_registry_ = std::make_unique<ToastRegistry>();
  toast_controller_ = std::make_unique<ToastController>(
      browser_window_interface, toast_registry_.get());
  RegisterToasts(browser_window_interface);
}

ToastService::~ToastService() = default;

void ToastService::RegisterToasts(
    BrowserWindowInterface* browser_window_interface) {
  CHECK(toast_registry_->IsEmpty());

  toast_registry_->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(kLinkChromeRefreshIcon,
                                  IDS_LINK_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kImageCopied,
      ToastSpecification::Builder(kCopyMenuIcon, IDS_IMAGE_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kLinkToHighlightCopied,
      ToastSpecification::Builder(kLinkChromeRefreshIcon,
                                  IDS_LINK_COPIED_TO_HIGHLIGHT_TOAST_BODY)
          .Build());
}
