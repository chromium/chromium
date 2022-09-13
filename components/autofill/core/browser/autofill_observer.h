// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace autofill {

class AutofillObserver : public base::CheckedObserver {
 public:
  enum NotificationType {
    AutocompleteFormSubmitted,
    AutocompleteFormSkipped,
    AutocompleteCleanupDone
  };

  // |notification_type| is the notification type that this observer observes.
  // |detach_on_notify| will let the AutofillSubject know that this
  // observer only wants to watch for the first notification of that type.
  AutofillObserver(NotificationType notification_type, bool detach_on_notify);

  // Invoked by the watched AutofillSubject.
  virtual void OnNotify() = 0;

  NotificationType notification_type() { return notification_type_; }
  bool detach_on_notify() { return detach_on_notify_; }

 private:
  NotificationType notification_type_;
  bool detach_on_notify_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OBSERVER_H_
