// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_PE_RESOURCE_H_
#define CHROME_UPDATER_WIN_INSTALLER_PE_RESOURCE_H_

#include <stddef.h>
#include <windows.h>

namespace updater {

// This class models a windows PE resource. It does not pretend to be a full
// API wrapper and it is just concerned with loading it to memory and writing
// it to disk. Each resource is unique only in the context of a loaded module,
// that is why you need to specify one on each constructor.
class PEResource {
 public:
  // Takes the resource name, the resource type, and the module where
  // to look for the resource. If the resource is found IsValid() returns true.
  PEResource(const wchar_t* name, const wchar_t* type, HMODULE module);

  // Returns true if the resource is valid.
  bool IsValid();

  // Returns the size in bytes of the resource. Returns zero if the resource is
  // not valid.
  size_t Size();

  // Creates a file in |path| with a copy of the resource. If the resource can
  // not be loaded into memory or if it cannot be written to disk it returns
  // false.
  bool WriteToDisk(const wchar_t* path);

 private:
  HRSRC resource_ = nullptr;
  HMODULE module_ = nullptr;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_PE_RESOURCE_H_
