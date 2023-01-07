// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_DECOMPRESS_H_
#define CHROME_INSTALLER_MINI_INSTALLER_DECOMPRESS_H_

namespace mini_installer {

// Same as the tool, expand.exe.  Decompresses a file that was compressed
// using Microsoft's MSCF compression algorithm.
// |source| is the full path of the file to decompress and |destination|
// is the full path of the target file.
bool Expand(const wchar_t* source, const wchar_t* destination);

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_DECOMPRESS_H_
