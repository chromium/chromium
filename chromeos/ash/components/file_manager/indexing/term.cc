// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/term.h"

#include "base/strings/utf_string_conversions.h"

namespace ash::file_manager {

Term::Term(const std::string& field, const std::u16string& token)
    : field_(field), token_(token), token_bytes_(base::UTF16ToUTF8(token)) {}

Term::~Term() = default;

}  // namespace ash::file_manager
