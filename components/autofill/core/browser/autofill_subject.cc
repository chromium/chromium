// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_subject.h"

#include <vector>

#include "base/observer_list.h"
#include "components/autofill/core/browser/autofill_observer.h"

namespace autofill {

AutofillSubject::AutofillSubject() = default;
AutofillSubject::~AutofillSubject() = default;

void AutofillSubject::Attach(AutofillObserver* observer) {
  observers_map_[observer->notification_type()].AddObserver(observer);
}

void AutofillSubject::Detach(AutofillObserver* observer) {
  auto it = observers_map_.find(observer->notification_type());
  if (it == observers_map_.end()) {
    return;
  }
  it->second.RemoveObserver(observer);
}

void AutofillSubject::Notify(NotificationType notification_type) {
  auto it = observers_map_.find(notification_type);
  if (it == observers_map_.end()) {
    return;
  }

  std::vector<AutofillObserver*> observers_to_remove;
  for (AutofillObserver& observer : it->second) {
    if (notification_type == observer.notification_type()) {
      observer.OnNotify();

      if (observer.detach_on_notify()) {
        observers_to_remove.push_back(&observer);
      }
    }
  }

  for (AutofillObserver* observer : observers_to_remove) {
    it->second.RemoveObserver(observer);
  }
}

}  // namespace autofill
