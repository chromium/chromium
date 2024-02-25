// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_H_
#define CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace updater {

class PrivilegedHelperServer;

class PrivilegedHelperService
    : public base::RefCountedThreadSafe<PrivilegedHelperService> {
 public:
  PrivilegedHelperService();

  void SetupSystemUpdater(const std::string& browser_path,
                          base::OnceCallback<void(int)> result);

 private:
  friend class base::RefCountedThreadSafe<PrivilegedHelperService>;

  virtual ~PrivilegedHelperService();

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
};

// Returns 0 if and only if the app bundle located at `browser_path` can be
// copied to a secure temp directory, verified, and then installed.
int InstallUpdater(const base::FilePath& browser_path);

// Returns true if and only if the app bundle located at `updater_app_bundle`
// is validly code signed with an updater identifier and appropriate team ID.
bool VerifyUpdaterSignature(const base::FilePath& updater_app_bundle);

}  // namespace updater

@interface PrivilegedHelperServiceXPCDelegate : NSObject <NSXPCListenerDelegate>
- (instancetype)
    initWithService:(scoped_refptr<updater::PrivilegedHelperService>)service
             server:(scoped_refptr<updater::PrivilegedHelperServer>)server;
@end

#endif  // CHROME_UPDATER_MAC_PRIVILEGED_HELPER_SERVICE_H_
