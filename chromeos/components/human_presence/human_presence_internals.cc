// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/human_presence/human_presence_internals.h"

namespace hps {

// Resource paths.
const char kHpsInternalsCSS[] = "human_presence_internals.css";
const char kHpsInternalsJS[] = "human_presence_internals.js";
const char kHpsInternalsIcon[] = "human_presence_internals_icon.svg";

// Message handlers.
const char kHpsInternalsConnectCmd[] = "connect";
const char kHpsInternalsEnableSenseCmd[] = "enable_sense";
const char kHpsInternalsDisableSenseCmd[] = "disable_sense";
const char kHpsInternalsQuerySenseCmd[] = "query_sense";
const char kHpsInternalsEnableNotifyCmd[] = "enable_notify";
const char kHpsInternalsDisableNotifyCmd[] = "disable_notify";
const char kHpsInternalsQueryNotifyCmd[] = "query_notify";

// Events.
const char kHpsInternalsConnectedEvent[] = "connected";
const char kHpsInternalsSenseChangedEvent[] = "sense_changed";
const char kHpsInternalsNotifyChangedEvent[] = "notify_changed";
const char kHpsInternalsEnableErrorEvent[] = "enable_error";

}  // namespace hps
