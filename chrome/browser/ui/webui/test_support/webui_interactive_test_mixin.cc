// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"

#include "ui/base/interaction/element_identifier.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebUiInteractiveTestMixinBase,
                                      kElementRenders);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebUiInteractiveTestMixinBase,
                                      kButtonWasClicked);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebUiInteractiveTestMixinBase,
                                      kIronCollapseContentShows);
