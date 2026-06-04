// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/ntp_custom_background_service_base.h"

#include "components/prefs/pref_service.h"
#include "components/themes/ntp_background_service.h"

NtpCustomBackgroundServiceBase::NtpCustomBackgroundServiceBase(
    PrefService* pref_service,
    NtpBackgroundService* background_service)
    : pref_service_(pref_service), background_service_(background_service) {}

NtpCustomBackgroundServiceBase::~NtpCustomBackgroundServiceBase() = default;

void NtpCustomBackgroundServiceBase::OnCollectionInfoAvailable() {}
void NtpCustomBackgroundServiceBase::OnCollectionImagesAvailable() {}
void NtpCustomBackgroundServiceBase::OnNextCollectionImageAvailable() {}
void NtpCustomBackgroundServiceBase::OnNtpBackgroundServiceShuttingDown() {}
