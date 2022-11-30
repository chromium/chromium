// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_CRX_FILE_H_
#define COMPONENTS_CRX_FILE_CRX_FILE_H_

namespace crx_file {

// The magic string embedded in the header.
constexpr char kCrxFileHeaderMagic[] = "Cr24";
constexpr char kCrxDiffFileHeaderMagic[] = "CrOD";
constexpr int kCrxFileHeaderMagicSize = 4;
constexpr char kSignatureContext[] = "CRX3 SignedData";

}  // namespace crx_file

#endif  // COMPONENTS_CRX_FILE_CRX_FILE_H_
