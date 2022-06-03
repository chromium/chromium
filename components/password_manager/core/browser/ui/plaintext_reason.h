// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PLAINTEXT_REASON_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PLAINTEXT_REASON_H_

namespace password_manager {

// Possible reasons why a plaintext password was requested.
enum class PlaintextReason {
  kView,
  kCopy,
  kEdit,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PLAINTEXT_REASON_H_
