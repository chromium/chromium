// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_EXCHANGE_DELEGATE_H_
#define COMPONENTS_EXO_DATA_EXCHANGE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace aura {
class Window;
}

namespace base {
class Pickle;
class RefCountedMemory;
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
  virtual ~DataExchangeDelegate() {}

  // Returns the endpoint type of `window`.
  virtual ui::EndpointType GetDataTransferEndpointType(
      aura::Window* window) const = 0;

  // Read filenames from text/uri-list |data| which was provided by |source|
  // endpoint. Translates paths from source to host format.
  virtual std::vector<ui::FileInfo> GetFilenames(
      ui::EndpointType source,
      const std::vector<uint8_t>& data) const = 0;

  // Returns the mime type which is used by |target| endpoint for a list of
  // file path URIs.
  virtual std::string GetMimeTypeForUriList(ui::EndpointType target) const = 0;

  // Sends the given list of |files| to |target| endpoint. Translates paths from
  // host format to the target and performs any required file sharing for VMs.
  using SendDataCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;
  virtual void SendFileInfo(ui::EndpointType target,
                            const std::vector<ui::FileInfo>& files,
                            SendDataCallback callback) const = 0;

  // Takes in |pickle| constructed by the web contents view and returns true if
  // it contains any valid filesystem URLs.
  virtual bool HasUrlsInPickle(const base::Pickle& pickle) const = 0;

  // Takes in |pickle| constructed by the web contents view containing
  // filesystem URLs. Provides translations for the specified |target| endpoint
  // and performs any required file sharing for VMs.
  virtual void SendPickle(ui::EndpointType target,
                          const base::Pickle& pickle,
                          SendDataCallback callback) = 0;

  // Reads pickle for FilesApp fs/sources with newline-separated filesystem
  // URLs. Validates that |source| is FilesApp.
  virtual std::vector<ui::FileInfo> ParseFileSystemSources(
      const ui::DataTransferEndpoint* source,
      const base::Pickle& pickle) const = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_EXCHANGE_DELEGATE_H_
