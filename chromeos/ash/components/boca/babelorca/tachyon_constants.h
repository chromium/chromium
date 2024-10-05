// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CONSTANTS_H_

namespace ash::babelorca {

// TODO(b/356929723): We should not launch with IntegTest. Set the right app
// name after onboarding to Tachyon.
inline constexpr char kTachyonAppName[] = "IntegTest";
inline constexpr char kSigninGaiaUrl[] =
    "https://instantmessaging-pa.googleapis.com/v1/registration:signingaia";
inline constexpr char kSendMessageUrl[] =
    "https://instantmessaging-pa.googleapis.com/v1/message:send";
inline constexpr char kReceiveMessagesUrl[] =
    "https://instantmessaging-pa.googleapis.com/v1/messages:receive";

inline constexpr char kOauthHeaderTemplate[] = "Authorization: Bearer %s";

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_CONSTANTS_H_
