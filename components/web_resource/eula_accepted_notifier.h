// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESOURCE_EULA_ACCEPTED_NOTIFIER_H_
#define COMPONENTS_WEB_RESOURCE_EULA_ACCEPTED_NOTIFIER_H_

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace web_resource {

// Helper class for querying the EULA accepted state and receiving a
// notification when the EULA is accepted.
class EulaAcceptedNotifier {
 public:
  // Observes EULA accepted state changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnEulaAccepted() = 0;
  };

  explicit EulaAcceptedNotifier(PrefService* local_state);
  virtual ~EulaAcceptedNotifier();

  // Initializes this class with the given |observer|. Must be called before
  // the class is used.
  void Init(Observer* observer);

  // Returns true if the EULA has been accepted. If the EULA has not yet been
  // accepted, begins monitoring the EULA state and will notify the observer
  // once the EULA has been accepted.
  virtual bool IsEulaAccepted();

  // Factory method for this class.
  static EulaAcceptedNotifier* Create(PrefService* local_state);

 protected:
  // Notifies the observer that the EULA has been updated, made protected for
  // testing.
  void NotifyObserver();

 private:
  // Callback for EULA accepted pref change notification.
  void OnPrefChanged();

  // Local state pref service for querying the EULA accepted pref.
  PrefService* local_state_;

  // Used to listen for the EULA accepted pref change notification.
  PrefChangeRegistrar registrar_;

  // Observer of the EULA accepted notification.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(EulaAcceptedNotifier);
};

}  // namespace web_resource

#endif  // COMPONENTS_WEB_RESOURCE_EULA_ACCEPTED_NOTIFIER_H_
