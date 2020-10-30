// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_FACTORY_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chromeos/components/camera_app_ui/camera_app_window_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}  // namespace content

namespace chromeos {

// Owns CameraAppWindowManager instances and associates them with Profiles. Note
// that the window manager can also work for app instance even if it is launched
// by typing URL under incognito mode.
class CameraAppWindowManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CameraAppWindowManager* GetForBrowserContext(
      content::BrowserContext* context);

  static CameraAppWindowManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CameraAppWindowManagerFactory>;

  CameraAppWindowManagerFactory();
  ~CameraAppWindowManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CameraAppWindowManagerFactory);
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_FACTORY_H_
