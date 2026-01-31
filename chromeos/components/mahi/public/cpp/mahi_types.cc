// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_types.h"

namespace chromeos {

MahiPageInfo::MahiPageInfo() = default;
MahiPageInfo::MahiPageInfo(const MahiPageInfo&) = default;
MahiPageInfo::MahiPageInfo(MahiPageInfo&&) noexcept = default;
MahiPageInfo& MahiPageInfo::operator=(const MahiPageInfo&) = default;
MahiPageInfo& MahiPageInfo::operator=(MahiPageInfo&&) noexcept = default;
MahiPageInfo::~MahiPageInfo() = default;

MahiContextMenuRequest::MahiContextMenuRequest() = default;
MahiContextMenuRequest::MahiContextMenuRequest(const MahiContextMenuRequest&) =
    default;
MahiContextMenuRequest::MahiContextMenuRequest(
    MahiContextMenuRequest&&) noexcept = default;
MahiContextMenuRequest& MahiContextMenuRequest::operator=(
    const MahiContextMenuRequest&) = default;
MahiContextMenuRequest& MahiContextMenuRequest::operator=(
    MahiContextMenuRequest&&) noexcept = default;
MahiContextMenuRequest::~MahiContextMenuRequest() = default;

MahiPageContent::MahiPageContent() = default;
MahiPageContent::MahiPageContent(const MahiPageContent&) = default;
MahiPageContent::MahiPageContent(MahiPageContent&&) noexcept = default;
MahiPageContent& MahiPageContent::operator=(const MahiPageContent&) = default;
MahiPageContent& MahiPageContent::operator=(MahiPageContent&&) noexcept =
    default;
MahiPageContent::~MahiPageContent() = default;

}  // namespace chromeos
