// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the interface for an iterator over a portable executable
// file's resources.

#ifndef CHROME_INSTALLER_TEST_PE_IMAGE_RESOURCES_H_
#define CHROME_INSTALLER_TEST_PE_IMAGE_RESOURCES_H_

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/check_op.h"

namespace base {
namespace win {
class PEImage;
}
}  // namespace base

namespace upgrade_test {

// A CopyConstructible and Assignable identifier for resource directory
// entries.
class EntryId {
 public:
  explicit EntryId(WORD number) : number_(number) {}
  explicit EntryId(const std::wstring& name) : name_(name), number_() {
    DCHECK_NE(static_cast<std::wstring::size_type>(0), name.size());
  }
  bool IsNamed() const { return !name_.empty(); }
  WORD number() const { return number_; }
  const std::wstring& name() const { return name_; }

 private:
  std::wstring name_;
  WORD number_;
};  // class EntryId

// A sequence of identifiers comprising the path from the root of an image's
// resource directory to an individual resource.
typedef std::vector<EntryId> EntryPath;

// A callback function invoked once for each data entry in the image's
// directory of resources.
// |path| - the full path of the data entry,
// |data| - the address of the entry's data.
// |size| - the size, in bytes, of the entry's data.
// |code_page| - the code page to be used to interpret string data in the
// entry's data.
// |context| - the context given to EnumResources.
typedef void (*EnumResource_Fn)(const EntryPath& path,
                                uint8_t* data,
                                DWORD size,
                                DWORD code_page,
                                uintptr_t context);

// Enumerates all data entries in |image|'s resource directory.  |callback| is
// invoked (and provided with |context|) once per entry.  Returns false if
// some or all of the resource directory could not be parsed.
bool EnumResources(const base::win::PEImage& image,
                   EnumResource_Fn callback,
                   uintptr_t context);

}  // namespace upgrade_test

#endif  // CHROME_INSTALLER_TEST_PE_IMAGE_RESOURCES_H_
