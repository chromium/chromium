// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/url_constants.h"

namespace ash {

const char kChromeUIMakoHost[] = "mako";
const char kChromeUIMakoURL[] = "chrome-untrusted://mako/";
const char kChromeUIMakoOrcaURL[] = "chrome-untrusted://mako/orca.html";
const char kChromeUIMakoPrivacyURL[] = "chrome-untrusted://mako/privacy.html";

const char kOrcaModeParamKey[] = "mode";
const char kOrcaWriteMode[] = "write";
const char kOrcaRewriteMode[] = "rewrite";

const char kOrcaPresetParamKey[] = "preset";
const char kOrcaFreeformParamKey[] = "freeform";

const char kOrcaFeedbackEnabledParamKey[] = "feedback-enabled";

const char kOrcaResizingEnabledParamKey[] = "resizing-enabled";

const char kOrcaMagicBoostParamKey[] = "magic-boost";

const char kOrcaHostLanguageParamKey[] = "hl";

const char kChromeUILobsterURL[] = "chrome-untrusted://mako/lobster.html";

const char kLobsterPromptParamKey[] = "prompt";

}  // namespace ash
