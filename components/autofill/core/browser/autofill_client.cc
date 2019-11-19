// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_client.h"

#include "components/version_info/channel.h"

namespace autofill {

AutofillClient::UnmaskDetails::UnmaskDetails() {}
AutofillClient::UnmaskDetails::~UnmaskDetails() {}

version_info::Channel AutofillClient::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

std::string AutofillClient::GetPageLanguage() const {
  return std::string();
}

LogManager* AutofillClient::GetLogManager() const {
  return nullptr;
}

bool AutofillClient::CloseWebauthnOfferDialog() {
  return false;
}

}  // namespace autofill
