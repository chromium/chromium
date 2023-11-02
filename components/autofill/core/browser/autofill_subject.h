// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUBJECT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUBJECT_H_

#include <map>

#include "base/observer_list.h"
#include "components/autofill/core/browser/autofill_observer.h"

namespace autofill {

// Subject that can emit notifications of specific types to observers that were
// opted-in.
class AutofillSubject {
 public:
  using NotificationType = AutofillObserver::NotificationType;

  AutofillSubject();
  ~AutofillSubject();

  void Attach(AutofillObserver* observer);
  void Detach(AutofillObserver* observer);

  // Will notify observers that are watching for the same |notification_type|.
  // This function is O(n^2) in case all observers are auto-detaching.
  void Notify(NotificationType notification_type);

 private:
  std::map<NotificationType, base::ObserverList<AutofillObserver>>
      observers_map_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUBJECT_H_
