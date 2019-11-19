// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_DLL_HASH_DLL_HASH_H_
#define CHROME_CHROME_ELF_DLL_HASH_DLL_HASH_H_

#include <string>

// Convert a dll name to a hash that can be sent via UMA.
int DllNameToHash(const std::string& dll_name);

#endif  // CHROME_CHROME_ELF_DLL_HASH_DLL_HASH_H_
