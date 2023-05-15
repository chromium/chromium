// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_COMMON_MAC_H_
#define COMPONENTS_SERVICES_QUARANTINE_COMMON_MAC_H_

#import <Foundation/Foundation.h>

namespace base {
class FilePath;
}

namespace quarantine {

NSDictionary* GetQuarantineProperties(const base::FilePath& file);

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_COMMON_MAC_H_
