// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_GET_FEEDBACK_DATA_CALLBACK_H_
#define CHROMEOS_LACROS_GET_FEEDBACK_DATA_CALLBACK_H_

#include "base/callback.h"
#include "base/values.h"

// Callback used by GetFeedbackData crosapi and all the downstream classes
// that propagate GetFeedbackData call for getting lacros feedback data.
using GetFeedbackDataCallback = base::OnceCallback<void(base::Value)>;

#endif  //  CHROMEOS_LACROS_GET_FEEDBACK_DATA_CALLBACK_H_
