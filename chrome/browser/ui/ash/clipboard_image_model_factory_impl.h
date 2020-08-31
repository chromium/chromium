// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_IMAGE_MODEL_FACTORY_IMPL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_IMAGE_MODEL_FACTORY_IMPL_H_

#include <string>

#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "base/unguessable_token.h"

class Profile;

// Implements the singleton ClipboardImageModelFactory.
class ClipboardImageModelFactoryImpl : public ash::ClipboardImageModelFactory {
 public:
  explicit ClipboardImageModelFactoryImpl(Profile* primary_profile);
  ClipboardImageModelFactoryImpl(ClipboardImageModelFactoryImpl&) = delete;
  ClipboardImageModelFactoryImpl& operator=(ClipboardImageModelFactoryImpl&) =
      delete;
  ~ClipboardImageModelFactoryImpl() override;

 private:
  // ash::ClipboardImageModelFactory:
  void Render(const base::UnguessableToken& id,
              const std::string& html_markup,
              ImageModelCallback callback) override;
  void CancelRequest(const base::UnguessableToken& id) override;
  void Activate() override;
  void Deactivate() override;

  // The primary profile, used instead of the active profile to create the
  // WebContents that renders html.
  Profile* const primary_profile_;
};

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_IMAGE_MODEL_FACTORY_IMPL_H_
