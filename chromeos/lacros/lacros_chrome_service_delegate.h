// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
#define CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/values.h"

class GURL;

namespace crosapi {
namespace mojom {
class LacrosInitParams;
}  // namespace mojom
}  // namespace crosapi

namespace chromeos {

// Interface to inject Chrome dependent behavior into LacrosChromeServiceImpl
// to split the dependency.
class LacrosChromeServiceDelegate {
 public:
  virtual ~LacrosChromeServiceDelegate() = default;

  // Called during startup when |init_params| become available.
  virtual void OnInitialized(
      const crosapi::mojom::LacrosInitParams& init_params) = 0;

  // Opens a new browser window.
  virtual void NewWindow() = 0;

  // Returns version of lacros-chrome displayed to user in feedback report, etc.
  // It includes both browser version and channel in the format of:
  // {browser version} {channel}
  // For example, "87.0.0.1 dev", "86.0.4240.38 beta".
  virtual std::string GetChromeVersion() = 0;

  using GetFeedbackDataCallback = base::OnceCallback<void(base::Value)>;
  // Gets lacros feedback data.
  virtual void GetFeedbackData(GetFeedbackDataCallback callback) = 0;

  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  // Gets lacros histograms.
  virtual void GetHistograms(GetHistogramsCallback callback) = 0;

  using GetActiveTabUrlCallback =
      base::OnceCallback<void(const base::Optional<GURL>&)>;
  // Gets Url of the active tab if there is any.
  virtual void GetActiveTabUrl(GetActiveTabUrlCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
