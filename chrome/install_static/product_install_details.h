// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions for determining the product's InstallDetails at runtime.

#ifndef CHROME_INSTALL_STATIC_PRODUCT_INSTALL_DETAILS_H_
#define CHROME_INSTALL_STATIC_PRODUCT_INSTALL_DETAILS_H_

#include <memory>
#include <string>

namespace install_static {

class PrimaryInstallDetails;

// Creates product details for the current process and sets them as the global
// InstallDetails for the process. This is intended to be called early in
// process startup. A process's "primary" module may be the executable itself or
// may be another DLL that is loaded and initialized prior to executing the
// executable's entrypoint (i.e., chrome_elf.dll).
void InitializeProductDetailsForPrimaryModule();

// Returns true if |parent| is a parent of |path|. Path separators at the end of
// |parent| are ignored. Returns false if |parent| is empty.
bool IsPathParentOf(const wchar_t* parent,
                    size_t parent_len,
                    const std::wstring& path);

// Returns true if |path| is within C:\Program Files{, (x86)}.
bool PathIsInProgramFiles(const std::wstring& path);

// Returns the install suffix embedded in |exe_path| or an empty string if none
// is found. |exe_path| is expected be something similar to
// "...\[kProductName][suffix]\Application".
std::wstring GetInstallSuffix(const std::wstring& exe_path);

// Creates product details for the process at |exe_path|.
std::unique_ptr<PrimaryInstallDetails> MakeProductDetails(
    const std::wstring& exe_path);

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_PRODUCT_INSTALL_DETAILS_H_
