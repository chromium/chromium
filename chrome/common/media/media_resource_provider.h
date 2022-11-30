// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_MEDIA_RESOURCE_PROVIDER_H_
#define CHROME_COMMON_MEDIA_MEDIA_RESOURCE_PROVIDER_H_

#include <string>

#include "media/base/localized_strings.h"

// This is called indirectly by the media layer to access resources.
std::u16string ChromeMediaLocalizedStringProvider(
    media::MessageId media_message_id);

#endif  // CHROME_COMMON_MEDIA_MEDIA_RESOURCE_PROVIDER_H_
