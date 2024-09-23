// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_APP_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_APP_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/image/image.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

// Represents an external app shown in sharing dialogs.
struct SharingApp {
 public:
  SharingApp(const gfx::VectorIcon* vector_icon,
             const gfx::Image& image,
             std::u16string name,
             std::string identifier);
  SharingApp(SharingApp&& other);

  SharingApp(const SharingApp&) = delete;
  SharingApp& operator=(const SharingApp&) = delete;

  ~SharingApp();

  raw_ptr<const gfx::VectorIcon> vector_icon = nullptr;
  gfx::Image image;
  std::u16string name;
  std::string identifier;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_APP_H_
