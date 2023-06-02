// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_ENUMERATE_RESOURCES_H_
#define CHROME_INSTALLER_MINI_INSTALLER_ENUMERATE_RESOURCES_H_

#include <windows.h>

namespace mini_installer {

struct MemoryRange;

class ResourceEnumeratorDelegate {
 public:
  ResourceEnumeratorDelegate(const ResourceEnumeratorDelegate&) = delete;
  ResourceEnumeratorDelegate& operator=(const ResourceEnumeratorDelegate&) =
      delete;

  // Processes the resource named `name` in the memory range indicated by
  // `data_range`, which is guaranteed to not be empty. Enumeration is aborted
  // when false is returned.
  // Note: This method is not pure virtual because that would require linking
  // in purecall handlers from the standard library.
  virtual bool OnResource(const wchar_t* name, const MemoryRange& data_range);

 protected:
  ResourceEnumeratorDelegate() = default;
  // Note: This class's destructor is not virtual because doing so requires
  // linking in bits of the standard library.
  ~ResourceEnumeratorDelegate() = default;
};

// Invokes `delegate`'s `OnResource` method for each resource of `type` in
// `module`. Returns true if the enumeration runs to completion; false in case
// of error or if the delegate halts enumeration prematurely.
bool EnumerateResources(ResourceEnumeratorDelegate&& delegate,
                        HMODULE module,
                        const wchar_t* type);

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_ENUMERATE_RESOURCES_H_
