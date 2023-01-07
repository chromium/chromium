// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_GENERATION_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_GENERATION_PROVIDER_H_

@protocol PasswordGenerationProvider <NSObject>

// Triggers password generation on the active field.
- (void)triggerPasswordGeneration;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_GENERATION_PROVIDER_H_
