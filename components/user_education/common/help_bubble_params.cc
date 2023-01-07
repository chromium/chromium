// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble_params.h"

namespace user_education {

HelpBubbleButtonParams::HelpBubbleButtonParams() = default;
HelpBubbleButtonParams::HelpBubbleButtonParams(HelpBubbleButtonParams&&) =
    default;
HelpBubbleButtonParams::~HelpBubbleButtonParams() = default;
HelpBubbleButtonParams& HelpBubbleButtonParams::operator=(
    HelpBubbleButtonParams&&) = default;

HelpBubbleParams::HelpBubbleParams() = default;
HelpBubbleParams::HelpBubbleParams(HelpBubbleParams&&) = default;
HelpBubbleParams::~HelpBubbleParams() = default;
HelpBubbleParams& HelpBubbleParams::operator=(HelpBubbleParams&&) = default;

}  // namespace user_education
