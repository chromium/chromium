// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for EME on android.
// Multiply-included message file, hence no include guard.
// no-include-guard-because-multiply-included

#include <vector>

#include "ipc/ipc_message_macros.h"
#include "media/base/eme_constants.h"

#define IPC_MESSAGE_START EncryptedMediaMsgStart

IPC_STRUCT_BEGIN(SupportedKeySystemRequest)
  IPC_STRUCT_MEMBER(std::string, key_system)
  IPC_STRUCT_MEMBER(media::SupportedCodecs, codecs, media::EME_CODEC_NONE)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(SupportedKeySystemResponse)
  IPC_STRUCT_MEMBER(std::string, key_system)
  IPC_STRUCT_MEMBER(media::SupportedCodecs, non_secure_codecs,
                    media::EME_CODEC_NONE)
  IPC_STRUCT_MEMBER(media::SupportedCodecs, secure_codecs,
                    media::EME_CODEC_NONE)
  IPC_STRUCT_MEMBER(bool, is_persistent_license_supported)
  IPC_STRUCT_MEMBER(bool, is_cbcs_encryption_supported)
IPC_STRUCT_END()

// Messages sent from the renderer to the browser.

// Synchronously query key system information. If the key system is supported,
// the response will be populated.
IPC_SYNC_MESSAGE_CONTROL1_1(
    ChromeViewHostMsg_QueryKeySystemSupport,
    SupportedKeySystemRequest /* key system information request */,
    SupportedKeySystemResponse /* key system information response */)

// Synchronously get a list of platform-supported EME key system names that
// are not explicitly handled by Chrome.
IPC_SYNC_MESSAGE_CONTROL0_1(
    ChromeViewHostMsg_GetPlatformKeySystemNames,
    std::vector<std::string> /* key system names */)
