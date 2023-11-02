// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_CLIENT_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_CLIENT_H_

#include "base/component_export.h"
#include "components/variations/variations.mojom.h"

namespace variations {

// Used by VariationsURLLoaderThrottle to insulate the content layer from
// concepts like user sign in which don't belong there. There is an instance per
// profile, so there can be multiple clients at a time when in multi user mode.
class COMPONENT_EXPORT(VARIATIONS) VariationsClient {
 public:
  virtual ~VariationsClient() = default;

  // Returns whether the user is operating in an OffTheRecord context.
  // Note components/variations code can't call the BrowserContext method
  // directly or we'd end up with a circular dependency.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the variations headers that may be appended to eligible requests
  // to Google web properties. For more details, see GetClientDataHeaders() in
  // variations_ids_provider.h.
  virtual variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const = 0;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_CLIENT_H_
