// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_INTERFACE_HOLDER_H_
#define COMPONENTS_TABS_PUBLIC_TAB_INTERFACE_HOLDER_H_

namespace tabs {

class TabInterface;

// A wrapper class for TabInterface its purpose is to abstract away memory
// management differences in tabs between platforms. This is able to be
// implemented as a 0-cost indirection on Desktop.
//
// The need for this class arises from the fact that on Desktop tabs are stored
// as `std::unique_ptr<TabModel>` whereas on Android we use `TabAndroid*`. The
// semantics of interacting with a unique_ptr and raw ptr are sufficiently
// different that a simple typedef is insufficient to resolve the difference and
// so a wrapper indirection is required.
class TabInterfaceHolder {
 public:
  TabInterfaceHolder() = default;
  virtual ~TabInterfaceHolder() = default;
  TabInterfaceHolder(const TabInterfaceHolder& other) = delete;
  void operator=(const TabInterfaceHolder& other) = delete;

  // Returns the stored `TabInterface`.
  virtual TabInterface* GetTabInterface() = 0;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_INTERFACE_HOLDER_H_
