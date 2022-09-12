// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_EPSON_DRIVER_MATCHING_H_
#define CHROMEOS_PRINTING_EPSON_DRIVER_MATCHING_H_

#include "base/component_export.h"

namespace chromeos {

struct PrinterSearchData;

// Implements PPD matching rules obtained from Epson. Returns true when this
// printer can be safely setup using the generic Epson PPD.
bool COMPONENT_EXPORT(CHROMEOS_PRINTING)
    CanUseEpsonGenericPPD(const PrinterSearchData& sd);

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_EPSON_DRIVER_MATCHING_H_
