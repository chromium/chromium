// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BOCA_ROLE_UTIL_H_
#define CHROME_BROWSER_ASH_BOCA_BOCA_ROLE_UTIL_H_

#include "components/prefs/pref_registry_simple.h"

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash::boca_util {
// Register prefs.
void RegisterPrefs(PrefRegistrySimple* registry);

// If boca role is producer.
bool IsProducer(const user_manager::User* user);

// If boca role is consumer.
bool IsConsumer(const user_manager::User* user);

// If boca is enabled.
bool IsEnabled(const user_manager::User* user);
}  // namespace ash::boca_util

#endif  // CHROME_BROWSER_ASH_BOCA_BOCA_ROLE_UTIL_H_
