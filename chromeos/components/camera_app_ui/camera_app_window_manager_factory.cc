// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/camera_app_ui/camera_app_window_manager_factory.h"

#include "base/macros.h"
#include "chromeos/components/camera_app_ui/camera_app_window_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

// static
CameraAppWindowManager* CameraAppWindowManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<CameraAppWindowManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
CameraAppWindowManagerFactory* CameraAppWindowManagerFactory::GetInstance() {
  return base::Singleton<CameraAppWindowManagerFactory>::get();
}

CameraAppWindowManagerFactory::CameraAppWindowManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "CameraAppWindowManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {}

CameraAppWindowManagerFactory::~CameraAppWindowManagerFactory() {}

KeyedService* CameraAppWindowManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new CameraAppWindowManager();
}

}  // namespace chromeos
