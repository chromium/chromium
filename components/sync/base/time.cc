// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/time.h"

#include <memory>

#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"

namespace syncer {

int64_t TimeToProtoTime(const base::Time& t) {
  return (t - base::Time::UnixEpoch()).InMilliseconds();
}

base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMilliseconds(proto_t);
}

std::string GetTimeDebugString(const base::Time& t) {
  // Note: We don't use some helper from base/i18n/time_formatting.h here,
  // because those are all locale-dependent which we explicitly don't want.
  UErrorCode status = U_ZERO_ERROR;
  icu::SimpleDateFormat formatter(icu::UnicodeString("yyyy-MM-dd HH:mm:ss X"),
                                  status);
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString date_string;
  formatter.format(static_cast<UDate>(t.ToDoubleT() * 1000), date_string);
  return base::UTF16ToUTF8(base::i18n::UnicodeStringToString16(date_string));
}

}  // namespace syncer
