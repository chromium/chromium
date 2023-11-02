// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALL_STATIC_INITIALIZE_FROM_PRIMARY_MODULE_H_
#define CHROME_INSTALL_STATIC_INITIALIZE_FROM_PRIMARY_MODULE_H_

namespace install_static {

// Initializes an InstallDetails instance for this module with the payload from
// the process's primary module (which exports an "GetInstallDetailsPayload"
// function for this express purpose).
void InitializeFromPrimaryModule();

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INITIALIZE_FROM_PRIMARY_MODULE_H_
