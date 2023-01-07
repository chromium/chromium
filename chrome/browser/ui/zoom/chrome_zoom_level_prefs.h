// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_CHROME_ZOOM_LEVEL_PREFS_H_
#define CHROME_BROWSER_UI_ZOOM_CHROME_ZOOM_LEVEL_PREFS_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/zoom_level_delegate.h"

namespace zoom {
class ZoomEventManager;
}

// A class to manage per-partition default and per-host zoom levels in Chrome's
// preference system. It implements an interface between the content/ zoom
// levels in HostZoomMap and Chrome's preference system. All changes
// to the per-partition default zoom levels from chrome/ flow through this
// class. Any changes to per-host levels are updated when HostZoomMap calls
// OnZoomLevelChanged.
class ChromeZoomLevelPrefs : public content::ZoomLevelDelegate {
 public:
  // Initialize the pref_service and the partition_key via the constructor,
  // as these concepts won't be available in the content base class
  // ZoomLevelDelegate, which will define the InitHostZoomMap interface.
  // |pref_service_| must outlive this class.
  ChromeZoomLevelPrefs(
      PrefService* pref_service,
      const base::FilePath& profile_path,
      const base::FilePath& partition_path,
      base::WeakPtr<zoom::ZoomEventManager> zoom_event_manager);

  ChromeZoomLevelPrefs(const ChromeZoomLevelPrefs&) = delete;
  ChromeZoomLevelPrefs& operator=(const ChromeZoomLevelPrefs&) = delete;

  ~ChromeZoomLevelPrefs() override;

  static std::string GetPartitionKeyForTesting(
      const base::FilePath& relative_path);

  void SetDefaultZoomLevelPref(double level);
  double GetDefaultZoomLevelPref() const;
  base::CallbackListSubscription RegisterDefaultZoomLevelCallback(
      base::RepeatingClosure callback);

  void ExtractPerHostZoomLevels(const base::Value::Dict& host_zoom_dictionary,
                                bool sanitize_partition_host_zoom_levels);

  // content::ZoomLevelDelegate
  void InitHostZoomMap(content::HostZoomMap* host_zoom_map) override;

 private:
  // This is a callback function that receives notifications from HostZoomMap
  // when per-host zoom levels change. It is used to update the per-host
  // zoom levels (if any) managed by this class (for its associated partition).
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  raw_ptr<PrefService> pref_service_;
  base::WeakPtr<zoom::ZoomEventManager> zoom_event_manager_;
  raw_ptr<content::HostZoomMap> host_zoom_map_;
  base::CallbackListSubscription zoom_subscription_;
  std::string partition_key_;
  base::RepeatingClosureList default_zoom_changed_callbacks_;
};

#endif  // CHROME_BROWSER_UI_ZOOM_CHROME_ZOOM_LEVEL_PREFS_H_
