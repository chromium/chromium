// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
#define CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/values.h"

class GURL;

namespace chromeos {

// Interface to inject Chrome dependent behavior into LacrosChromeServiceImpl
// to split the dependency.
// TODO(hidehiko): This interface is being removed. Please do not add
// any new methods.
class LacrosChromeServiceDelegate {
 public:
  virtual ~LacrosChromeServiceDelegate() = default;

  // Opens a new browser window.
  virtual void NewWindow(bool incognito) = 0;

  // Opens a new browser tab.
  virtual void NewTab() = 0;

  // Restores a tab recently closed.
  virtual void RestoreTab() = 0;

  using GetFeedbackDataCallback = base::OnceCallback<void(base::Value)>;
  // Gets lacros feedback data.
  virtual void GetFeedbackData(
      GetFeedbackDataCallback callback) = 0;

  // Gets lacros histograms.
  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  virtual void GetHistograms(
      GetHistogramsCallback callback) = 0;

  // Gets Url of the active tab if there is any.
  virtual GURL GetActiveTabUrl() = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
