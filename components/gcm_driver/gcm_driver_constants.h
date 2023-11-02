// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the GCM(Google Cloud
// Messaging) Driver.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_CONSTANTS_H_
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_CONSTANTS_H_

#include "base/files/file_path.h"

namespace gcm_driver {

// File path for GCM Store.
extern const base::FilePath::CharType kGCMStoreDirname[];

}  //  namespace gcm_driver

#endif  //  COMPONENTS_GCM_DRIVER_GCM_DRIVER_CONSTANTS_H_
