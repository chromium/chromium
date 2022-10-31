// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATION_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATION_DATA_H_

#include "base/files/file_path.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace web_app {

// Contains IWA-specific information like bundle location. All IWAs will have
// an instance of this struct in their WebApp object.
struct IsolationData {
  struct InstalledBundle {
    bool operator==(const InstalledBundle& other) const;
    bool operator!=(const InstalledBundle& other) const;

    base::FilePath path;
  };

  struct DevModeBundle {
    bool operator==(const DevModeBundle& other) const;
    bool operator!=(const DevModeBundle& other) const;

    base::FilePath path;
  };

  struct DevModeProxy {
    bool operator==(const DevModeProxy& other) const;
    bool operator!=(const DevModeProxy& other) const;

    url::Origin proxy_url;
  };

  explicit IsolationData(
      absl::variant<InstalledBundle, DevModeBundle, DevModeProxy> content);
  ~IsolationData();
  IsolationData(const IsolationData&);
  IsolationData& operator=(const IsolationData&);
  IsolationData(IsolationData&&);
  IsolationData& operator=(IsolationData&&);

  bool operator==(const IsolationData&) const;
  bool operator!=(const IsolationData&) const;
  base::Value AsDebugValue() const;

  absl::variant<InstalledBundle, DevModeBundle, DevModeProxy> content;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATION_DATA_H_
