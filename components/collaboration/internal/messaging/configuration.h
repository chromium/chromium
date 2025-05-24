// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_CONFIGURATION_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_CONFIGURATION_H_

namespace collaboration::messaging {

// The MessagingBackendConfiguration provides details about how the
// MessagingBackendService should behave, and provides a way to inject
// specialized behavior, for example platform specific behavior, without
// needing to reason about platforms in the code, only configuration.
struct MessagingBackendConfiguration {
 public:
  MessagingBackendConfiguration();
  MessagingBackendConfiguration(const MessagingBackendConfiguration&);
  MessagingBackendConfiguration& operator=(
      const MessagingBackendConfiguration&);
  ~MessagingBackendConfiguration();

  // When true, the backend clears the PersistentMessage for tab chips when a
  // tab is selected. When false, the backend clears the the chip when a tab
  // is unselected after first having been selected, which is the case for
  // desktop platforms.
  bool clear_chip_on_tab_selection = true;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_CONFIGURATION_H_
