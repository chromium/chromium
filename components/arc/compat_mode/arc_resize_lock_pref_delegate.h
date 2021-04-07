// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_PREF_DELEGATE_H_
#define COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_PREF_DELEGATE_H_

#include <string>

namespace arc {

class ArcResizeLockPrefDelegate {
 public:
  virtual ~ArcResizeLockPrefDelegate() = default;

  virtual bool GetResizeLockNeedsConfirmation(const std::string& app_id) = 0;
  virtual void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                              bool is_needed) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_PREF_DELEGATE_H_
