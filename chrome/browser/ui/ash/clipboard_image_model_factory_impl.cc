// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_image_model_factory_impl.h"

#include "chrome/browser/profiles/profile.h"

ClipboardImageModelFactoryImpl::ClipboardImageModelFactoryImpl(
    Profile* primary_profile)
    : primary_profile_(primary_profile) {
  DCHECK(primary_profile_);
}

ClipboardImageModelFactoryImpl::~ClipboardImageModelFactoryImpl() = default;

void ClipboardImageModelFactoryImpl::Render(const base::UnguessableToken& id,
                                            const std::string& html_markup,
                                            ImageModelCallback callback) {
  std::move(callback).Run(ui::ImageModel());
}

void ClipboardImageModelFactoryImpl::CancelRequest(
    const base::UnguessableToken& id) {}

void ClipboardImageModelFactoryImpl::Activate() {}

void ClipboardImageModelFactoryImpl::Deactivate() {}
