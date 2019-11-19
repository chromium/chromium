// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_UTILS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_UTILS_H_

#include "base/strings/string16.h"
#include "chrome/browser/webauthn/authenticator_transport.h"

namespace gfx {
struct VectorIcon;
}

enum class TransportSelectionContext {
  kTransportSelectionSheet,
  kOtherTransportsMenu,
};

// Returns the human readable name shown for |transport| in the given |context|.
base::string16 GetTransportHumanReadableName(AuthenticatorTransport transport,
                                             TransportSelectionContext context);

// Returns the vector icon to show next to the |transport| in the manual
// transport selection list.
const gfx::VectorIcon* GetTransportVectorIcon(AuthenticatorTransport transport);

#endif  // CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_UTILS_H_
