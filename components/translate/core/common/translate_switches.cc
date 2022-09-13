// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_switches.h"

namespace translate {
namespace switches {

// Overrides the default server used for Google Translate.
const char kTranslateScriptURL[] = "translate-script-url";

// Overrides security-origin with which Translate runs in an isolated world.
const char kTranslateSecurityOrigin[] = "translate-security-origin";

// Overrides the URL from which the translate ranker model is downloaded.
const char kTranslateRankerModelURL[] = "translate-ranker-model-url";

}  // namespace switches
}  // namespace translate
