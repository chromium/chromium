// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_HPS_HPS_INTERNALS_H_
#define CHROMEOS_COMPONENTS_HPS_HPS_INTERNALS_H_

namespace hps {

// Resource paths.
extern const char kHpsInternalsCSS[];
extern const char kHpsInternalsJS[];
extern const char kHpsInternalsIcon[];

// Message handlers.
extern const char kHpsInternalsConnectCmd[];
extern const char kHpsInternalsEnableSenseCmd[];
extern const char kHpsInternalsDisableSenseCmd[];
extern const char kHpsInternalsQuerySenseCmd[];
extern const char kHpsInternalsEnableNotifyCmd[];
extern const char kHpsInternalsDisableNotifyCmd[];
extern const char kHpsInternalsQueryNotifyCmd[];

// Events.
extern const char kHpsInternalsConnectedEvent[];
extern const char kHpsInternalsSenseChangedEvent[];
extern const char kHpsInternalsNotifyChangedEvent[];
extern const char kHpsInternalsEnableErrorEvent[];

}  // namespace hps

#endif  // CHROMEOS_COMPONENTS_HPS_HPS_INTERNALS_H_
