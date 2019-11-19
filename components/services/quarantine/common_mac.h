// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_COMMON_MAC_H_
#define COMPONENTS_SERVICES_QUARANTINE_COMMON_MAC_H_

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"

namespace base {
class FilePath;
}

namespace quarantine {

bool GetQuarantineProperties(
    const base::FilePath& file,
    base::scoped_nsobject<NSMutableDictionary>* properties);

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_COMMON_MAC_H_
