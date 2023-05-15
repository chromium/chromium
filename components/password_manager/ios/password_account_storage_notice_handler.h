// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_ACCOUNT_STORAGE_NOTICE_HANDLER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_ACCOUNT_STORAGE_NOTICE_HANDLER_H_

#include "base/ios/block_types.h"

// Protocol for account storage notice.
// TODO(crbug.com/1434606): Remove this when the move to account storage notice
// is removed.
@protocol PasswordsAccountStorageNoticeHandler

// Whether to show the one-time notice that passwords stored in the signed-in
// account might be offered as suggestions.
- (BOOL)shouldShowAccountStorageNotice;

// Must only be called if shouldShowAccountStorageNotice: is true.
- (void)showAccountStorageNotice:(ProceduralBlock)completion;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_ACCOUNT_STORAGE_NOTICE_HANDLER_H_
