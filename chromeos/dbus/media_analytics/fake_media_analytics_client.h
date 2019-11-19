// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MEDIA_ANALYTICS_FAKE_MEDIA_ANALYTICS_CLIENT_H_
#define CHROMEOS_DBUS_MEDIA_ANALYTICS_FAKE_MEDIA_ANALYTICS_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/media_perception/media_perception.pb.h"

namespace chromeos {

// MediaAnalyticsClient is used to communicate with a media analytics process
// running outside of Chrome.
class COMPONENT_EXPORT(MEDIA_ANALYTICS_CLIENT) FakeMediaAnalyticsClient
    : public MediaAnalyticsClient {
 public:
  FakeMediaAnalyticsClient();
  ~FakeMediaAnalyticsClient() override;

  // Checks that a FakeMediaAnalyticsClient instance was initialized and returns
  // it.
  static FakeMediaAnalyticsClient* Get();

  // Inherited from MediaAnalyticsClient.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetState(DBusMethodCallback<mri::State> callback) override;
  void SetState(const mri::State& state,
                DBusMethodCallback<mri::State> callback) override;
  void GetDiagnostics(DBusMethodCallback<mri::Diagnostics> callback) override;
  void BootstrapMojoConnection(base::ScopedFD file_descriptor,
                               VoidDBusMethodCallback callback) override;

  // Fires a fake media perception event.
  bool FireMediaPerceptionEvent(const mri::MediaPerception& media_perception);

  // Sets the object to be returned from GetDiagnostics.
  void SetDiagnostics(const mri::Diagnostics& diagnostics);

  // Sets the state of the media analytics process to SUSPENDED.
  void SetStateSuspended();

  void set_process_running(bool running) { process_running_ = running; }

  bool process_running() const { return process_running_; }

 private:
  // Echoes back the previously set state.
  void OnState(DBusMethodCallback<mri::State> callback);

  // Runs callback with the Diagnostics proto provided in SetDiagnostics.
  void OnGetDiagnostics(DBusMethodCallback<mri::Diagnostics> callback);

  // Notifies observers with a MediaPerception proto provided in
  // FireMediaPerceptionEvent.
  void OnMediaPerception(const mri::MediaPerception& media_perception);

  // Observers for receiving MediaPerception proto messages.
  base::ObserverList<Observer>::Unchecked observer_list_;

  // A fake current state for the media analytics process.
  mri::State current_state_;

  // A fake diagnostics object to be returned by the GetDiagnostics.
  mri::Diagnostics diagnostics_;

  // Whether the fake media analytics was started (for example by the fake
  // upstart client) - If not set, all requests to this client will fail.
  bool process_running_;

  base::WeakPtrFactory<FakeMediaAnalyticsClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMediaAnalyticsClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MEDIA_ANALYTICS_FAKE_MEDIA_ANALYTICS_CLIENT_H_
