// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_WRL_MODULE_H_
#define CHROME_UPDATER_WIN_WRL_MODULE_H_

#if !defined(__WRL_CLASSIC_COM_STRICT__)
#error "WRL must not depend on WinRT."
#endif

#include <wrl/module.h>

#endif  // CHROME_UPDATER_WIN_WRL_MODULE_H_
