// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_
#define COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/themes/ntp_background_service_observer.h"

class PrefService;
class NtpBackgroundService;

// Shared base class for managing custom backgrounds on the NTP.
// Encapsulates platform-agnostic preference management and NtpBackgroundService
// interactions.
class NtpCustomBackgroundServiceBase : public KeyedService,
                                       public NtpBackgroundServiceObserver {
 public:
  NtpCustomBackgroundServiceBase(PrefService* pref_service,
                                 NtpBackgroundService* background_service);
  ~NtpCustomBackgroundServiceBase() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

 protected:
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  raw_ptr<NtpBackgroundService, DanglingUntriaged> background_service_;
};

#endif  // COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_
