// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_CPP_LIBASSISTANT_LOADER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_CPP_LIBASSISTANT_LOADER_H_

#include "base/callback_helpers.h"
#include "base/component_export.h"

namespace chromeos::libassistant {

// Interface to load libassistant library for different versions.
class COMPONENT_EXPORT(LIBASSISTANT_PUBLIC_LOADER) LibassistantLoader {
 public:
  using LoadCallback = base::OnceCallback<void(bool success)>;

  virtual ~LibassistantLoader() = default;

  // Load the libassistant.so.
  // `callback` will be called to indicate whether loading is successful.
  static void Load(LoadCallback callback);
};

}  // namespace chromeos::libassistant

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_PUBLIC_CPP_LIBASSISTANT_LOADER_H_
