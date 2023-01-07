// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_observer.h"

#include "base/run_loop.h"

namespace autofill {

AutofillObserver::AutofillObserver(NotificationType notification_type,
                                   bool detach_on_notify)
    : notification_type_(notification_type),
      detach_on_notify_(detach_on_notify) {}

}  // namespace autofill
