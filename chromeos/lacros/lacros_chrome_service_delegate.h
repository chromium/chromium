// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
#define CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_

#include <string>

namespace chromeos {

// Interface to inject Chrome dependent behavior into LacrosChromeServiceImpl
// to split the dependency.
class LacrosChromeServiceDelegate {
 public:
  virtual ~LacrosChromeServiceDelegate() = default;

  // Opens a new browser window.
  virtual void NewWindow() = 0;

  // Returns version of lacros-chrome displayed to user in feedback report, etc.
  // It includes both browser version and channel in the format of:
  // {browser version} {channel}
  // For example, "87.0.0.1 dev", "86.0.4240.38 beta".
  virtual std::string GetChromeVersion() = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
