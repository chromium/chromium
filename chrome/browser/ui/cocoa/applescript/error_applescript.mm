// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"

#import <Foundation/Foundation.h>

#include "base/notreached.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace AppleScript {

void SetError(Error error_code) {
  NSScriptCommand* current_command = [NSScriptCommand currentCommand];
  current_command.scriptErrorNumber = static_cast<int>(error_code);

  NSString* error_string = @"";
  switch (error_code) {
    case Error::kGetProfile:
      error_string =
          l10n_util::GetNSString(IDS_GET_PROFILE_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kBookmarkModelLoad:
      error_string =
          l10n_util::GetNSString(IDS_BOOKMARK_MODEL_LOAD_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kCreateBookmarkFolder:
      error_string = l10n_util::GetNSString(
          IDS_CREATE_BOOKMARK_FOLDER_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kCreateBookmarkItem:
      error_string = l10n_util::GetNSString(
          IDS_CREATE_BOOKMARK_ITEM_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kInvalidURL:
      error_string = l10n_util::GetNSString(IDS_INVALID_URL_APPLESCRIPT_MAC);
      break;
    case Error::kInitiatePrinting:
      error_string =
          l10n_util::GetNSString(IDS_INITIATE_PRINTING_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kInvalidSaveType:
      error_string =
          l10n_util::GetNSString(IDS_INVALID_SAVE_TYPE_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kInvalidMode:
      error_string =
          l10n_util::GetNSString(IDS_INVALID_MODE_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kInvalidTabIndex:
      error_string =
          l10n_util::GetNSString(IDS_INVALID_TAB_INDEX_APPLESCRIPT_MAC);
      break;
    case Error::kSetMode:
      error_string = l10n_util::GetNSString(IDS_SET_MODE_APPLESCRIPT_MAC);
      break;
    case Error::kWrongIndex:
      error_string =
          l10n_util::GetNSString(IDS_WRONG_INDEX_ERROR_APPLESCRIPT_MAC);
      break;
    case Error::kJavaScriptUnsupported:
      error_string = l10n_util::GetNSString(
          IDS_JAVASCRIPT_UNSUPPORTED_ERROR_APPLESCRIPT_MAC);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  current_command.scriptErrorString = error_string;
}

}  // namespace AppleScript
