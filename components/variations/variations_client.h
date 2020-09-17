// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_CLIENT_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_CLIENT_H_

namespace variations {

// Used by VariationsURLLoaderThrottle to insulate the content layer from
// concepts like user sign in which don't belong there. There is an instance per
// profile, so there can be multiple clients at a time when in multi user mode.
class VariationsClient {
 public:
  virtual ~VariationsClient() = default;

  // Returns whether the user is operating in an OffTheRecord context.
  // Note components/variations code can't call the BrowserContext method
  // directly or we'd end up with a circular dependency.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the variations header that should be appended for google requests.
  // TODO(crbug/1094303): Update the signature to take a
  // variations::Study_GoogleWebVisibility.
  virtual std::string GetVariationsHeader() const = 0;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_CLIENT_H_
