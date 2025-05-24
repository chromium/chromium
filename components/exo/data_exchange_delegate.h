// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_EXCHANGE_DELEGATE_H_
#define COMPONENTS_EXO_DATA_EXCHANGE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace aura {
class Window;
}

namespace base {
class Pickle;
}  // namespace base

namespace ui {
class DataTransferEndpoint;
struct FileInfo;
enum class EndpointType;
}  // namespace ui

namespace exo {

// Interface for data exchange operations that are implemented in chrome such as
// file drag and drop path translations and file sharing for VMs.
class DataExchangeDelegate {
 public:
  virtual ~DataExchangeDelegate() = default;

  // Returns the endpoint type of `window`.
  virtual ui::EndpointType GetDataTransferEndpointType(
      aura::Window* window) const = 0;

  // Returns the mime type which is used by |target| endpoint for a list of
  // file path URIs.
  virtual std::string GetMimeTypeForUriList(ui::EndpointType target) const = 0;

  // Takes in |pickle| constructed by the web contents view and returns true if
  // it contains any valid filesystem URLs.
  virtual bool HasUrlsInPickle(const base::Pickle& pickle) const = 0;

  // Reads pickle for FilesApp fs/sources with newline-separated filesystem
  // URLs. Validates that |source| is FilesApp.
  virtual std::vector<ui::FileInfo> ParseFileSystemSources(
      const ui::DataTransferEndpoint* source,
      const base::Pickle& pickle) const = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_EXCHANGE_DELEGATE_H_
