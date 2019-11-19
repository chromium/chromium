// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include <stddef.h>
#include <algorithm>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/translate/core/common/translate_switches.h"
#include "url/gurl.h"

namespace translate {

const char kSecurityOrigin[] = "https://translate.googleapis.com/";

GURL GetTranslateSecurityOrigin() {
  std::string security_origin(kSecurityOrigin);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTranslateSecurityOrigin)) {
    security_origin =
        command_line->GetSwitchValueASCII(switches::kTranslateSecurityOrigin);
  }
  return GURL(security_origin);
}

}  // namespace translate
