// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_FACTORY_AND_CLSID_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_FACTORY_AND_CLSID_H_

#include <wrl/client.h>

#include "base/win/windows_types.h"

struct FactoryAndClsid {
  FactoryAndClsid();
  ~FactoryAndClsid();

  Microsoft::WRL::ComPtr<IClassFactory> factory;
  CLSID clsid;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_FACTORY_AND_CLSID_H_
