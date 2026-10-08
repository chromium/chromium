// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/constants.h"

namespace client_certificates {

const char kManagedProfileIdentityName[] = "ManagedProfileIdentity";

const char kManagedBrowserIdentityName[] = "ManagedBrowserIdentity";

const char kTemporaryManagedProfileIdentityName[] =
    "TemporaryManagedProfileIdentityName";

const char kTemporaryManagedBrowserIdentityName[] =
    "TemporaryManagedBrowserIdentityName";

const char kKey[] = "PrivateKey";

const char kKeySource[] = "PrivateKeySource";

const char kKeyDetails[] = "KeyDetails";

const char kCertificate[] = "Certificate";

const int kDaysBeforeExpiration = 7;

const char kCommonNameSourceKey[] = "common_name_source";

const char kCommonNameSourceDeviceId[] = "device_id";

const char kCommonNameSourceProfileId[] = "profile_id";

const char kCommonNameSourceComputerName[] = "computer_name";

}  // namespace client_certificates
