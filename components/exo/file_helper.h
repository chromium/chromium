// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_FILE_HELPER_H_
#define COMPONENTS_EXO_FILE_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"

namespace aura {
class Window;
}

namespace base {
class Pickle;
class RefCountedMemory;
}  // namespace base

namespace ui {
struct FileInfo;
}  // namespace ui

namespace exo {

// Handles file-related translations for wayland clipboard and drag-and-drop.
class FileHelper {
 public:
  virtual ~FileHelper() {}

  // Read filenames from |data| which was provided by source window |source|.
  // Translates paths from source to host format.
  virtual std::vector<ui::FileInfo> GetFilenames(
      aura::Window* source,
      const std::vector<uint8_t>& data) const = 0;

  // Returns the mime type which is used by target window |target| for a list of
  // file path URIs.
  virtual std::string GetMimeTypeForUriList(aura::Window* target) const = 0;

  // Sends the given file list |files| to target window |target| window.
  // Translates paths from host format to the target and performs any required
  // file sharing for VMs.
  using SendDataCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;
  virtual void SendFileInfo(aura::Window* target,
                            const std::vector<ui::FileInfo>& files,
                            SendDataCallback callback) const = 0;

  // Takes in |pickle| constructed by the web contents view and returns true if
  // it contains any valid filesystem URLs.
  virtual bool HasUrlsInPickle(const base::Pickle& pickle) const = 0;

  // Takes in |pickle| constructed by the web contents view containing
  // filesystem URLs. Provides translations for the specified target window
  // |target| and performs any required file sharing for VMs..
  virtual void SendPickle(aura::Window* target,
                          const base::Pickle& pickle,
                          SendDataCallback callback) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_FILE_HELPER_H_
