// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_dialog_data.h"

SharingDialogData::SharingDialogData() = default;

SharingDialogData::~SharingDialogData() = default;

SharingDialogData::SharingDialogData(SharingDialogData&& other) = default;

SharingDialogData& SharingDialogData::operator=(SharingDialogData&& other) =
    default;

SharingDialogData::HeaderIcons::HeaderIcons(const gfx::VectorIcon* light,
                                            const gfx::VectorIcon* dark)
    : light(light), dark(dark) {}
