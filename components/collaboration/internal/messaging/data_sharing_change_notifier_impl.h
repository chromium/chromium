// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace collaboration::messaging {

class DataSharingChangeNotifierImpl : public DataSharingChangeNotifier {
 public:
  explicit DataSharingChangeNotifierImpl(
      data_sharing::DataSharingService* data_sharing_service);
  ~DataSharingChangeNotifierImpl() override;

  // DataSharingChangeNotifier.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Initialize() override;
  bool IsInitialized() override;

  // DataSharingService::Observer.
  void OnGroupDataModelLoaded() override;

 private:
  // Informs observers that this has been initialized. This is in a separate
  // method to ensure that we can post this if the DataSharingService is already
  // initialized when we try to initialize this class.
  void NotifyDataSharingChangeNotifierInitialized() const;

  // Whether this has already been initialized.
  bool is_initialized_ = false;

  // Our scoped observer of the DataSharingService. Using ScopedObservation
  // simplifies our destruction logic.
  base::ScopedObservation<data_sharing::DataSharingService,
                          data_sharing::DataSharingService::Observer>
      data_sharing_service_observer_{this};

  // The list of observers observing this particular class.
  base::ObserverList<Observer> observers_;

  // The DataSharingService that is the source of the updates.
  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  base::WeakPtrFactory<DataSharingChangeNotifierImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_IMPL_H_
