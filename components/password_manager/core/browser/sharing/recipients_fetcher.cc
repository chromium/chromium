// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/recipients_fetcher.h"

namespace password_manager {

RecipientInfo::RecipientInfo() = default;

RecipientInfo::RecipientInfo(const RecipientInfo& other) = default;
RecipientInfo::RecipientInfo(RecipientInfo&& other) = default;
RecipientInfo& RecipientInfo::operator=(const RecipientInfo&) = default;
RecipientInfo& RecipientInfo::operator=(RecipientInfo&&) = default;
RecipientInfo::~RecipientInfo() = default;

bool RecipientInfo::operator==(const RecipientInfo& other) const {
  return user_id == other.user_id && user_name == other.user_name &&
         email == other.email && profile_image_url == other.profile_image_url;
}

}  // namespace password_manager
