// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"

#include "base/notreached.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

void AppleScript::SetError(AppleScript::ErrorCode errorCode) {
  using l10n_util::GetNSString;
  NSScriptCommand* current_command = [NSScriptCommand currentCommand];
  [current_command setScriptErrorNumber:(int)errorCode];
  NSString* error_string = @"";
  switch (errorCode) {
    case errGetProfile:
      error_string = GetNSString(IDS_GET_PROFILE_ERROR_APPLESCRIPT_MAC);
      break;
    case errBookmarkModelLoad:
      error_string = GetNSString(IDS_BOOKMARK_MODEL_LOAD_ERROR_APPLESCRIPT_MAC);
      break;
    case errCreateBookmarkFolder:
      error_string =
          GetNSString(IDS_CREATE_BOOKMARK_FOLDER_ERROR_APPLESCRIPT_MAC);
        break;
    case errCreateBookmarkItem:
      error_string =
          GetNSString(IDS_CREATE_BOOKMARK_ITEM_ERROR_APPLESCRIPT_MAC);
        break;
    case errInvalidURL:
      error_string = GetNSString(IDS_INVALID_URL_APPLESCRIPT_MAC);
      break;
    case errInitiatePrinting:
      error_string = GetNSString(IDS_INITIATE_PRINTING_ERROR_APPLESCRIPT_MAC);
      break;
    case errInvalidSaveType:
      error_string = GetNSString(IDS_INVALID_SAVE_TYPE_ERROR_APPLESCRIPT_MAC);
      break;
    case errInvalidMode:
      error_string = GetNSString(IDS_INVALID_MODE_ERROR_APPLESCRIPT_MAC);
      break;
    case errInvalidTabIndex:
      error_string = GetNSString(IDS_INVALID_TAB_INDEX_APPLESCRIPT_MAC);
      break;
    case errSetMode:
      error_string = GetNSString(IDS_SET_MODE_APPLESCRIPT_MAC);
      break;
    case errWrongIndex:
      error_string = GetNSString(IDS_WRONG_INDEX_ERROR_APPLESCRIPT_MAC);
      break;
    case errJavaScriptUnsupported:
      error_string =
          GetNSString(IDS_JAVASCRIPT_UNSUPPORTED_ERROR_APPLESCRIPT_MAC);
      break;
    default:
      NOTREACHED();
  }
  [current_command setScriptErrorString:error_string];
}
