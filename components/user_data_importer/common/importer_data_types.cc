// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/common/importer_data_types.h"

#include "build/build_config.h"

namespace user_data_importer {

SourceProfile::SourceProfile()
    : importer_type(TYPE_UNKNOWN), services_supported(0) {}

SourceProfile::SourceProfile(const SourceProfile& other) = default;

SourceProfile::~SourceProfile() = default;

ImporterIE7PasswordInfo::ImporterIE7PasswordInfo() = default;

ImporterIE7PasswordInfo::ImporterIE7PasswordInfo(
    const ImporterIE7PasswordInfo& other) = default;

ImporterIE7PasswordInfo::~ImporterIE7PasswordInfo() = default;

ImporterIE7PasswordInfo& ImporterIE7PasswordInfo::operator=(
    const ImporterIE7PasswordInfo& other) = default;

ImportedPasswordForm::ImportedPasswordForm() = default;

ImportedPasswordForm::ImportedPasswordForm(const ImportedPasswordForm& form) =
    default;

ImportedPasswordForm::ImportedPasswordForm(
    ImportedPasswordForm&& form) noexcept = default;

ImportedPasswordForm& ImportedPasswordForm::operator=(
    const ImportedPasswordForm& form) = default;

ImportedPasswordForm& ImportedPasswordForm::operator=(
    ImportedPasswordForm&& form) = default;

ImportedPasswordForm::~ImportedPasswordForm() = default;

}  // namespace user_data_importer
