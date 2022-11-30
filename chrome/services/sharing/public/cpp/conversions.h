// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_PUBLIC_CPP_CONVERSIONS_H_
#define CHROME_SERVICES_SHARING_PUBLIC_CPP_CONVERSIONS_H_

#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"

namespace sharing {

nearby::FileMetadata_Type ConvertFileMetadataType(
    mojom::FileMetadata::Type type);

nearby::TextMetadata_Type ConvertTextMetadataType(
    mojom::TextMetadata::Type type);

nearby::WifiCredentialsMetadata_SecurityType ConvertWifiCredentialsMetadataType(
    mojom::WifiCredentialsMetadata::SecurityType type);

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_PUBLIC_CPP_CONVERSIONS_H_
