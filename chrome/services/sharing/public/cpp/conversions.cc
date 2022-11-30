// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/public/cpp/conversions.h"

namespace sharing {

nearby::FileMetadata_Type ConvertFileMetadataType(
    mojom::FileMetadata::Type type) {
  switch (type) {
    case mojom::FileMetadata::Type::kImage:
      return nearby::FileMetadata_Type_IMAGE;
    case mojom::FileMetadata::Type::kVideo:
      return nearby::FileMetadata_Type_VIDEO;
    case mojom::FileMetadata::Type::kApp:
      return nearby::FileMetadata_Type_APP;
    case mojom::FileMetadata::Type::kAudio:
      return nearby::FileMetadata_Type_AUDIO;
    case mojom::FileMetadata::Type::kUnknown:
      return nearby::FileMetadata_Type_UNKNOWN;
  }
}

nearby::TextMetadata_Type ConvertTextMetadataType(
    mojom::TextMetadata::Type type) {
  switch (type) {
    case mojom::TextMetadata::Type::kText:
      return nearby::TextMetadata_Type_TEXT;
    case mojom::TextMetadata::Type::kUrl:
      return nearby::TextMetadata_Type_URL;
    case mojom::TextMetadata::Type::kAddress:
      return nearby::TextMetadata_Type_ADDRESS;
    case mojom::TextMetadata::Type::kPhoneNumber:
      return nearby::TextMetadata_Type_PHONE_NUMBER;
    case mojom::TextMetadata::Type::kUnknown:
      return nearby::TextMetadata_Type_UNKNOWN;
  }
}

nearby::WifiCredentialsMetadata_SecurityType ConvertWifiCredentialsMetadataType(
    mojom::WifiCredentialsMetadata::SecurityType type) {
  switch (type) {
    case mojom::WifiCredentialsMetadata::SecurityType::kOpen:
      return nearby::WifiCredentialsMetadata_SecurityType_OPEN;
    case mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk:
      return nearby::WifiCredentialsMetadata_SecurityType_WPA_PSK;
    case mojom::WifiCredentialsMetadata::SecurityType::kWep:
      return nearby::WifiCredentialsMetadata_SecurityType_WEP;
    case mojom::WifiCredentialsMetadata::SecurityType::kUnknownSecurityType:
      return nearby::WifiCredentialsMetadata_SecurityType_UNKNOWN_SECURITY_TYPE;
  }
}

}  // namespace sharing
