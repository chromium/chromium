// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/fullscreen_control/fullscreen_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace exclusive_access_bubble {

namespace {

// Helper function to categorize if the bubble type requires hold to exit.
bool IsHoldRequiredToExit(ExclusiveAccessBubbleType type) {
  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
      return true;
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
      return false;
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
      return base::FeatureList::IsEnabled(
          features::kPressAndHoldEscToExitBrowserFullscreen);
    default:
      NOTREACHED();
  }
}

}  // namespace

std::u16string GetInstructionTextForType(ExclusiveAccessBubbleType type,
                                         const std::u16string& accelerator,
                                         const url::Origin& origin,
                                         bool has_download,
                                         bool notify_overridden) {
  if (has_download) {
    if (notify_overridden) {
      return IsHoldRequiredToExit(type)
                 ? l10n_util::GetStringFUTF16(
                       IDS_FULLSCREEN_HOLD_TO_SEE_DOWNLOADS_AND_EXIT,
                       accelerator)
                 : l10n_util::GetStringFUTF16(
                       IDS_FULLSCREEN_PRESS_TO_SEE_DOWNLOADS_AND_EXIT,
                       accelerator);
    }
    return IsHoldRequiredToExit(type)
               ? l10n_util::GetStringFUTF16(
                     IDS_FULLSCREEN_HOLD_TO_SEE_DOWNLOADS, accelerator)
               : l10n_util::GetStringFUTF16(
                     IDS_FULLSCREEN_PRESS_TO_SEE_DOWNLOADS, accelerator);
  }

  std::optional<std::u16string> origin_string;
  if (!origin.opaque() &&
      base::FeatureList::IsEnabled(features::kFullscreenBubbleShowOrigin)) {
    origin_string = url_formatter::FormatOriginForSecurityDisplay(
        origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  }

  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION:
      // Both tab fullscreen and tab fullscreen + pointer lock have the same
      // message (the user does not care about pointer lock when in fullscreen
      // mode). All ways to trigger fullscreen result in the same message.
      if (origin_string) {
        return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_PRESS_TO_EXIT_FULLSCREEN_WITH_ORIGIN,
            origin_string.value(), accelerator);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_PRESS_TO_EXIT_FULLSCREEN, accelerator);
      }

    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
      if (origin_string) {
        return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_HOLD_TO_EXIT_FULLSCREEN_WITH_ORIGIN,
            origin_string.value(), accelerator);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_HOLD_TO_EXIT_FULLSCREEN, accelerator);
      }
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION:
      if (origin_string) {
        return l10n_util::GetStringFUTF16(
            IDS_PRESS_TO_EXIT_MOUSELOCK_WITH_ORIGIN, origin_string.value(),
            accelerator);
      } else {
        return l10n_util::GetStringFUTF16(IDS_PRESS_TO_EXIT_MOUSELOCK,
                                          accelerator);
      }
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
      if (base::FeatureList::IsEnabled(
              features::kPressAndHoldEscToExitBrowserFullscreen)) {
        return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_HOLD_TO_EXIT_FULLSCREEN, accelerator);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_FULLSCREEN_PRESS_TO_EXIT_FULLSCREEN, accelerator);
      }
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
    default:
      NOTREACHED();
  }
}

bool IsExclusiveAccessModeBrowserFullscreen(ExclusiveAccessBubbleType type) {
  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
      return true;
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION:
      return false;
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace exclusive_access_bubble
