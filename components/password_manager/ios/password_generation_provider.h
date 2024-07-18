// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_GENERATION_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_GENERATION_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/common/unique_ids.h"

namespace web {
class WebFrame;
}

@protocol PasswordGenerationProvider <NSObject>

// Triggers password generation on the active field.
- (void)triggerPasswordGeneration;

// Triggers proactive password generation on a field.
- (void)triggerPasswordGenerationForFormId:
            (autofill::FormRendererId)formIdentifier
                           fieldIdentifier:
                               (autofill::FieldRendererId)fieldIdentifier
                                   inFrame:(web::WebFrame*)frame
                                 proactive:(BOOL)proactivePasswordGeneration;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_GENERATION_PROVIDER_H_
