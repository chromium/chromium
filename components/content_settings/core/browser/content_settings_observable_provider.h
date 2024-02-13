// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_

#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_provider.h"

namespace content_settings {

class PartitionKey;

class ObservableProvider : public ProviderInterface {
 public:
  ObservableProvider();
  ~ObservableProvider() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // See `content_settings::Observer` for details.
  void NotifyObservers(const ContentSettingsPattern& primary_pattern,
                       const ContentSettingsPattern& secondary_pattern,
                       ContentSettingsType content_type,
                       const PartitionKey* partition_key);
  void RemoveAllObservers();
  bool CalledOnValidThread();

 private:
  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer, true>::Unchecked observer_list_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_
