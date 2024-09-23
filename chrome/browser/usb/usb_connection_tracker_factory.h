// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CONNECTION_TRACKER_FACTORY_H_
#define CHROME_BROWSER_USB_USB_CONNECTION_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/usb/usb_connection_tracker.h"

class UsbConnectionTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static UsbConnectionTracker* GetForProfile(Profile* profile, bool create);
  static UsbConnectionTrackerFactory* GetInstance();

  UsbConnectionTrackerFactory(const UsbConnectionTrackerFactory&) = delete;
  UsbConnectionTrackerFactory& operator=(const UsbConnectionTrackerFactory&) =
      delete;

 private:
  friend base::NoDestructor<UsbConnectionTrackerFactory>;

  UsbConnectionTrackerFactory();
  ~UsbConnectionTrackerFactory() override;

  // BrowserContextKeyedBaseFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_USB_USB_CONNECTION_TRACKER_FACTORY_H_
