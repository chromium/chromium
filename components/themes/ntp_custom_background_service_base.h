// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_
#define COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/themes/ntp_background_data.h"
#include "components/themes/ntp_background_service_observer.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class PrefService;
class NtpBackgroundService;

namespace base {
class FilePath;
}  // namespace base

// Shared base class for managing custom backgrounds on the NTP.
// Encapsulates platform-agnostic preference management and NtpBackgroundService
// interactions.
class NtpCustomBackgroundServiceBase : public KeyedService,
                                       public NtpBackgroundServiceObserver {
 public:
  NtpCustomBackgroundServiceBase(
      PrefService* pref_service,
      NtpBackgroundService* background_service,
      const std::string& custom_background_dict_pref_name,
      const std::string& local_to_device_pref_name);
  ~NtpCustomBackgroundServiceBase() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // Adds/Removes NtpCustomBackgroundServiceObserver observers.
  virtual void AddObserver(NtpCustomBackgroundServiceObserver* observer);
  virtual void RemoveObserver(NtpCustomBackgroundServiceObserver* observer);

  // Invoked when the background is reset on the NTP.
  // Virtual for testing.
  virtual void ResetCustomBackgroundInfo();

  // Invoked when a background pref update is received via sync, triggering
  // an update of theme info.
  // Virtual for testing.
  virtual void UpdateBackgroundFromSync();

  // Invoked when a custom background is configured on the NTP.
  // Virtual for testing.
  virtual void SetCustomBackgroundInfo(const GURL& background_url,
                                       const GURL& thumbnail_url,
                                       const std::string& attribution_line_1,
                                       const std::string& attribution_line_2,
                                       const GURL& action_url,
                                       const std::string& collection_id);

  // Invoked when a user selected the "Upload an image" option on the NTP.
  // Virtual for testing.
  virtual void SelectLocalBackgroundImage(const base::FilePath& path) = 0;

  // Virtual for testing.
  virtual void RefreshBackgroundIfNeeded();

  // Virtual for testing.
  virtual std::optional<CustomBackground> GetCustomBackground();

  // Updates the current custom background preference with the extracted color
  // if the current background URL matches |image_url|. Returns true if the
  // preference was updated, false otherwise.
  // Virtual for testing and platform-specific notifications.
  virtual bool UpdateCustomBackgroundPrefsWithColor(const GURL& image_url,
                                                    SkColor color);

 protected:
  // Returns the timestamp for the next daily refresh. Returns std::nullopt if
  // daily refresh is not enabled.Subclasses can override this to provide a fake
  // timestamp if they handle daily refresh scheduling elsewhere (e.g. Android).
  virtual std::optional<int> GetNextRefreshTimestamp() const;

  static base::DictValue NtpCustomBackgroundDefaults();
  // Returns false if the custom background pref cannot be parsed, otherwise
  // returns true.
  bool IsCustomBackgroundPrefValid();
  static base::DictValue GetBackgroundInfoAsDict(
      const GURL& background_url,
      const std::string& attribution_line_1,
      const std::string& attribution_line_2,
      const GURL& action_url,
      const std::optional<std::string>& collection_id,
      const std::optional<std::string>& resume_token,
      std::optional<int> refresh_timestamp);

  virtual void NotifyAboutBackgrounds();

  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  raw_ptr<NtpBackgroundService, DanglingUntriaged> background_service_;

 private:
  PrefChangeRegistrar pref_change_registrar_;
  const std::string custom_background_dict_pref_name_;
  const std::string local_to_device_pref_name_;

  base::ScopedObservation<NtpBackgroundService, NtpBackgroundServiceObserver>
      background_service_observation_{this};
  base::ObserverList<NtpCustomBackgroundServiceObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NtpCustomBackgroundServiceBase> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_
