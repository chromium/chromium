// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/seal_utils.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/logging.h"

namespace {

  constexpr char kSealKeyHash[] = 
      "\x58\x68\x46\x8c\x87\x23\x66\x2b\xef\x20\x58\xc5\x27\x2b\xcf\x0e"
      "\x13\x27\xea\xc1";

}

namespace ash {

bool SealUtils::ShouldEnable() {
  const std::string seal_key_hash_from_user = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kSealKey));
  return seal_key_hash_from_user == kSealKeyHash;
}

}  // namespace ash
