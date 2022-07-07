// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_STORAGE_PREFS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_STORAGE_PREFS_H_

class PrefRegistrySimple;

namespace storage {

extern const char kWebSQLAccess[];

extern const char kWebSQLNonSecureContextEnabled[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_STORAGE_PREFS_H_
