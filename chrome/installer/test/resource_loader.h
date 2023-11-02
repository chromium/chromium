// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper class for loading resources out of portable executable files.

#ifndef CHROME_INSTALLER_TEST_RESOURCE_LOADER_H_
#define CHROME_INSTALLER_TEST_RESOURCE_LOADER_H_

#include <windows.h>

#include <stdint.h>

#include <string>
#include <utility>

namespace base {
class FilePath;
}

namespace upgrade_test {

// Loads resources in a PE image file.
class ResourceLoader {
 public:
  ResourceLoader();

  ResourceLoader(const ResourceLoader&) = delete;
  ResourceLoader& operator=(const ResourceLoader&) = delete;

  ~ResourceLoader();

  // Loads |pe_image_path| in preparation for loading its resources.
  bool Initialize(const base::FilePath& pe_image_path);

  // Places the address and size of the resource |name| of |type| into
  // |resource_data|, returning true on success.  The address of the resource is
  // valid only until this instance is destroyed.
  bool Load(const std::wstring& name,
            const std::wstring& type,
            std::pair<const uint8_t*, DWORD>* resource_data);

  // Places the address and size of the resource |id| of |type| into
  // |resource_data|, returning true on success.  The address of the resource is
  // valid only until this instance is destroyed.
  bool Load(WORD id,
            WORD type,
            std::pair<const uint8_t*, DWORD>* resource_data);

 private:
  HMODULE module_;
};  // class ResourceLoader

}  // namespace upgrade_test

#endif  // CHROME_INSTALLER_TEST_RESOURCE_LOADER_H_
