// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_PROTOCOL_H_
#define CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_PROTOCOL_H_

#import <Foundation/Foundation.h>

// Protocol for the XPC privileged helper service.
@protocol PrivilegedHelperServiceProtocol <NSObject>

- (void)setupSystemUpdaterWithBrowserPath:(NSString* _Nonnull)browserPath
                                    reply:(void (^_Nonnull)(int rc))reply;
@end

#endif  // CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_PROTOCOL_H_
