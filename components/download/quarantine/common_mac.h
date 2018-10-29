// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_QUARANTINE_COMMON_MAC_H_
#define COMPONENTS_DOWNLOAD_QUARANTINE_COMMON_MAC_H_

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"

namespace base {
class FilePath;
}

namespace download {

bool GetQuarantinePropertiesDeprecated(
    const base::FilePath& file,
    base::scoped_nsobject<NSMutableDictionary>* properties);

bool GetQuarantineProperties(
    const base::FilePath& file,
    base::scoped_nsobject<NSMutableDictionary>* properties);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_QUARANTINE_COMMON_MAC_H_
