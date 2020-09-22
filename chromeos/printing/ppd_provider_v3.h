// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_PROVIDER_V3_H_
#define CHROMEOS_PRINTING_PPD_PROVIDER_V3_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "base/version.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/ppd_provider.h"

namespace chromeos {
// Creates and returns a new v3 PpdProvider.
//
// TODO(crbug.com/888189): fold this into ppd_provider.h,
// deprecating the existing static Create() method.
CHROMEOS_EXPORT scoped_refptr<PpdProvider> CreateV3Provider(
    const base::Version& current_version,
    scoped_refptr<PpdCache> cache,
    std::unique_ptr<PpdMetadataManager> metadata_manager,
    std::unique_ptr<PrinterConfigCache> config_cache);

// TODO(crbug.com/888189): make this free function a static method
//
// Used to "dereference" the PPD previously named by the cache key from
// Printer::PpdReference::effective_make_and_model.
CHROMEOS_EXPORT std::string PpdBasenameToCacheKey(
    base::StringPiece ppd_basename);

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_PROVIDER_V3_H_
