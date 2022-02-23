// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_H_
#define CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"

namespace base {
class SequencedTaskRunner;
}

namespace updater {

class PrivilegedHelperServer;

class PrivilegedHelperService
    : public base::RefCountedThreadSafe<PrivilegedHelperService> {
 public:
  PrivilegedHelperService();

  void SetupSystemUpdater(const std::string& browser_path,
                          base::OnceCallback<void(int)> result);

 protected:
  friend class base::RefCountedThreadSafe<PrivilegedHelperService>;

  virtual ~PrivilegedHelperService();

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
};

}  // namespace updater

@interface PrivilegedHelperServiceXPCDelegate : NSObject <NSXPCListenerDelegate>
- (instancetype)
    initWithService:(scoped_refptr<updater::PrivilegedHelperService>)service
             server:(scoped_refptr<updater::PrivilegedHelperServer>)server;
@end

#endif  // CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_H_
