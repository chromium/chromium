// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_PROPERTIES_SUCCESS_BARRIER_CALLBACK_H_
#define COMPONENTS_DBUS_PROPERTIES_SUCCESS_BARRIER_CALLBACK_H_

#include <cstddef>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

// A callback wrapper that must be called |num_calls| times with an argument of
// true (indicates success) for |done_callback| to be called with true.  If the
// wrapper is called with false, |done_callback| is immediately run with an
// argument of false.  Further calls after |done_callback| has already been run
// will have no effect.
COMPONENT_EXPORT(DBUS)
base::RepeatingCallback<void(bool)> SuccessBarrierCallback(
    size_t num_calls,
    base::OnceCallback<void(bool)> done_callback);

#endif  // COMPONENTS_DBUS_PROPERTIES_SUCCESS_BARRIER_CALLBACK_H_
