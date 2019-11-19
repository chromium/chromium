// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_INPUT_TRACE_H_
#define COMPONENTS_EXO_INPUT_TRACE_H_

#include "base/time/time.h"
#include "ui/events/event.h"

#define TRACE_EXO_INPUT_EVENT(event)                                \
  TRACE_EVENT2("exo", "Input::OnInputEvent", "type", event->type(), \
               "timestamp",                                         \
               (event->time_stamp() - base::TimeTicks()).InMicroseconds());

#endif  // COMPONENTS_EXO_INPUT_TRACE_H_
